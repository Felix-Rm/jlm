/*
 * Copyright 2021 David Metz <david.c.metz@ntnu.no>
 * See COPYING for terms of redistribution.
 */

#include <jlm/hls/backend/rvsdg2rhls/mem-conv.hpp>
#include <jlm/hls/backend/rvsdg2rhls/mem-sep.hpp>
#include <jlm/hls/ir/hls.hpp>
#include <jlm/llvm/ir/operators/call.hpp>
#include <jlm/llvm/ir/operators/lambda.hpp>
#include <jlm/llvm/ir/operators/Load.hpp>
#include <jlm/llvm/ir/operators/MemoryStateOperations.hpp>
#include <jlm/llvm/ir/operators/Store.hpp>
#include <jlm/rvsdg/gamma.hpp>
#include <jlm/rvsdg/substitution.hpp>
#include <jlm/rvsdg/theta.hpp>
#include <jlm/rvsdg/traverser.hpp>
#include <jlm/rvsdg/view.hpp>

#include <algorithm>

namespace jlm::hls
{

void
mem_sep_independent(llvm::RvsdgModule & rm)
{
  auto & graph = rm.Rvsdg();
  auto root = &graph.GetRootRegion();
  mem_sep_independent(root);
}

void
mem_sep_argument(llvm::RvsdgModule & rm)
{
  auto & graph = rm.Rvsdg();
  auto root = &graph.GetRootRegion();
  mem_sep_argument(root);
}

// from MemoryStateEncoder.cpp
rvsdg::RegionArgument *
GetMemoryStateArgument(const rvsdg::LambdaNode & lambda)
{
  auto subregion = lambda.subregion();
  for (size_t n = 0; n < subregion->narguments(); n++)
  {
    auto argument = subregion->argument(n);
    if (jlm::rvsdg::is<jlm::llvm::MemoryStateType>(argument->type()))
      return argument;
  }
  return nullptr;
}

rvsdg::RegionResult *
GetMemoryStateResult(const rvsdg::LambdaNode & lambda)
{
  auto subregion = lambda.subregion();
  for (size_t n = 0; n < subregion->nresults(); n++)
  {
    auto result = subregion->result(n);
    if (jlm::rvsdg::is<jlm::llvm::MemoryStateType>(result->type()))
      return result;
  }

  JLM_UNREACHABLE("This should have never happened!");
}

void
gather_mem_nodes(rvsdg::Region * region, std::vector<jlm::rvsdg::SimpleNode *> & mem_nodes)
{
  for (auto & node : rvsdg::TopDownTraverser(region))
  {
    if (auto structnode = dynamic_cast<rvsdg::StructuralNode *>(node))
    {
      for (size_t n = 0; n < structnode->nsubregions(); n++)
        gather_mem_nodes(structnode->subregion(n), mem_nodes);
    }
    else if (auto simplenode = dynamic_cast<jlm::rvsdg::SimpleNode *>(node))
    {
      if (dynamic_cast<const llvm::StoreNonVolatileOperation *>(&simplenode->GetOperation()))
      {
        mem_nodes.push_back(simplenode);
      }
      else if (dynamic_cast<const llvm::LoadNonVolatileOperation *>(&simplenode->GetOperation()))
      {
        mem_nodes.push_back(simplenode);
      }
    }
  }
}

jlm::rvsdg::output *
route_through(rvsdg::Region * target, jlm::rvsdg::output * response)
{
  if (response->region() == target)
  {
    return response;
  }
  else
  {
    auto parent_response = route_through(target->node()->region(), response);
    auto parrent_user = *parent_response->begin();
    if (auto gn = dynamic_cast<rvsdg::GammaNode *>(target->node()))
    {
      auto ip = gn->AddEntryVar(parent_response);
      parrent_user->divert_to(gn->AddExitVar(ip.branchArgument).output);
      for (auto arg : ip.branchArgument)
      {
        if (arg->region() == target)
        {
          return arg;
        }
      }
      JLM_UNREACHABLE("THIS SHOULD NOT HAPPEN");
    }
    else if (auto tn = dynamic_cast<rvsdg::ThetaNode *>(target->node()))
    {
      auto lv = tn->AddLoopVar(parent_response);
      parrent_user->divert_to(lv.output);
      return lv.pre;
    }
    JLM_UNREACHABLE("THIS SHOULD NOT HAPPEN");
  }
}

/* assign each load and store its own state edge. */
void
mem_sep_independent(rvsdg::Region * region)
{
  auto lambda = dynamic_cast<const rvsdg::LambdaNode *>(region->Nodes().begin().ptr());
  auto lambda_region = lambda->subregion();
  auto state_arg = GetMemoryStateArgument(*lambda);
  if (!state_arg)
  {
    // no memstate - i.e. no memory used
    return;
  }
  auto state_user = *state_arg->begin();
  std::vector<jlm::rvsdg::SimpleNode *> mem_nodes;
  gather_mem_nodes(lambda_region, mem_nodes);
  auto entry_states =
      jlm::llvm::LambdaEntryMemoryStateSplitOperation::Create(*state_arg, 1 + mem_nodes.size());
  auto state_result = GetMemoryStateResult(*lambda);
  // handle existing state edge - TODO: remove entirely?
  state_user->divert_to(entry_states.back());
  entry_states.pop_back();
  entry_states.push_back(state_result->origin());
  auto & merged_state =
      jlm::llvm::LambdaExitMemoryStateMergeOperation::Create(*lambda_region, entry_states);
  entry_states.pop_back();
  state_result->divert_to(&merged_state);
  for (auto node : mem_nodes)
  {
    auto in_state = route_through(node->region(), entry_states.back());
    auto out_state = *in_state->begin();
    auto node_input = node->input(node->ninputs() - 1);
    auto old_in_state = node_input->origin();
    node_input->divert_to(in_state);
    auto node_output = node->output(node->noutputs() - 1);
    JLM_ASSERT(node_output->nusers() == 1);
    node->output(node->noutputs() - 1)->divert_users(old_in_state);
    out_state->divert_to(node_output);
    entry_states.pop_back();
  }
}

rvsdg::RegionResult *
trace_edge(
    jlm::rvsdg::output * common_edge,
    jlm::rvsdg::output * new_edge,
    std::vector<jlm::rvsdg::SimpleNode *> & load_nodes,
    const std::vector<jlm::rvsdg::SimpleNode *> & store_nodes,
    std::vector<jlm::rvsdg::SimpleNode *> & decouple_nodes)
{
  // follows along common edge and routes new edge through the same regions
  // redirects the supplied loads, stores and decouples to the new edge
  // the new edge might be routed through unnecessary regions. This should be fixed by running DNE
  while (true)
  {
    // each iteration should update common_edge and/or new_edge
    JLM_ASSERT(common_edge->nusers() == 1);
    JLM_ASSERT(new_edge->nusers() == 1);
    auto user = *common_edge->begin();
    auto new_next = *new_edge->begin();
    if (auto res = dynamic_cast<rvsdg::RegionResult *>(user))
    {
      // end of region reached
      return res;
    }
    else if (auto gammaNode = rvsdg::TryGetOwnerNode<rvsdg::GammaNode>(*user))
    {
      auto ip = gammaNode->AddEntryVar(new_edge);
      std::vector<jlm::rvsdg::output *> vec;
      new_edge = gammaNode->AddExitVar(ip.branchArgument).output;
      new_next->divert_to(new_edge);

      auto entryvar = gammaNode->MapInputEntryVar(*user);
      for (size_t i = 0; i < gammaNode->nsubregions(); ++i)
      {
        auto subres = trace_edge(
            entryvar.branchArgument[i],
            ip.branchArgument[i],
            load_nodes,
            store_nodes,
            decouple_nodes);
        common_edge = subres->output();
      }
    }
    else if (auto theta = rvsdg::TryGetOwnerNode<rvsdg::ThetaNode>(*user))
    {
      auto olv = theta->MapInputLoopVar(*user);
      auto lv = theta->AddLoopVar(new_edge);
      trace_edge(olv.pre, lv.pre, load_nodes, store_nodes, decouple_nodes);
      common_edge = olv.output;
      new_edge = lv.output;
      new_next->divert_to(new_edge);
    }
    else if (auto si = dynamic_cast<jlm::rvsdg::simple_input *>(user))
    {
      auto sn = si->node();
      auto op = &si->node()->GetOperation();
      if (dynamic_cast<const jlm::llvm::StoreNonVolatileOperation *>(op))
      {
        JLM_ASSERT(sn->noutputs() == 1);
        if (store_nodes.end() != std::find(store_nodes.begin(), store_nodes.end(), sn))
        {
          si->divert_to(new_edge);
          sn->output(0)->divert_users(common_edge);
          new_edge = sn->output(0);
          new_next->divert_to(new_edge);
        }
        else
        {
          common_edge = sn->output(0);
        }
      }
      else if (dynamic_cast<const jlm::llvm::LoadNonVolatileOperation *>(op))
      {
        JLM_ASSERT(sn->noutputs() == 2);
        if (load_nodes.end() != std::find(load_nodes.begin(), load_nodes.end(), sn))
        {
          auto new_next = *new_edge->begin();
          si->divert_to(new_edge);
          sn->output(1)->divert_users(common_edge);
          new_next->divert_to(sn->output(1));
          new_edge = sn->output(1);
          new_next->divert_to(new_edge);
        }
        else
        {
          common_edge = sn->output(1);
        }
      }
      else if (dynamic_cast<const jlm::llvm::CallOperation *>(op))
      {
        JLM_ASSERT("Decoupled nodes not implemented yet");
      }
      else
      {
        JLM_ASSERT(sn->noutputs() == 1);
        common_edge = sn->output(0);
      }
    }
    else
    {
      JLM_UNREACHABLE("THIS SHOULD NOT HAPPEN");
    }
  }
}

/* assign each pointer argument its own state edge. */
void
mem_sep_argument(rvsdg::Region * region)
{
  auto lambda = dynamic_cast<const rvsdg::LambdaNode *>(region->Nodes().begin().ptr());
  auto lambda_region = lambda->subregion();
  auto state_arg = GetMemoryStateArgument(*lambda);
  if (!state_arg)
  {
    // no memstate - i.e., no memory used
    return;
  }
  auto state_user = *state_arg->begin();
  port_load_store_decouple port_nodes;
  TracePointerArguments(lambda, port_nodes);
  auto entry_states =
      jlm::llvm::LambdaEntryMemoryStateSplitOperation::Create(*state_arg, 1 + port_nodes.size());
  auto state_result = GetMemoryStateResult(*lambda);
  // handle existing state edge - TODO: remove entirely?
  auto common_edge = entry_states.back();
  entry_states.pop_back();
  state_user->divert_to(common_edge);
  entry_states.push_back(state_result->origin());
  auto & merged_state =
      jlm::llvm::LambdaExitMemoryStateMergeOperation::Create(*lambda_region, entry_states);
  entry_states.pop_back();
  state_result->divert_to(&merged_state);
  for (auto tp : port_nodes)
  {
    auto new_edge = entry_states.back();
    entry_states.pop_back();
    trace_edge(common_edge, new_edge, std::get<0>(tp), std::get<1>(tp), std::get<2>(tp));
  }
}

} // namespace jlm::hls

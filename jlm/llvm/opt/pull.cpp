/*
 * Copyright 2017 Nico Reißmann <nico.reissmann@gmail.com>
 * See COPYING for terms of redistribution.
 */

#include <jlm/llvm/ir/operators.hpp>
#include <jlm/llvm/ir/RvsdgModule.hpp>
#include <jlm/llvm/opt/pull.hpp>
#include <jlm/rvsdg/gamma.hpp>
#include <jlm/rvsdg/traverser.hpp>
#include <jlm/util/Statistics.hpp>
#include <jlm/util/time.hpp>

namespace jlm::llvm
{

class pullstat final : public util::Statistics
{
public:
  ~pullstat() override = default;

  explicit pullstat(const util::filepath & sourceFile)
      : Statistics(Statistics::Id::PullNodes, sourceFile)
  {}

  void
  start(const jlm::rvsdg::graph & graph) noexcept
  {
    AddMeasurement(Label::NumRvsdgInputsBefore, rvsdg::ninputs(graph.root()));
    AddTimer(Label::Timer).start();
  }

  void
  end(const jlm::rvsdg::graph & graph) noexcept
  {
    AddMeasurement(Label::NumRvsdgInputsAfter, rvsdg::ninputs(graph.root()));
    GetTimer(Label::Timer).stop();
  }

  static std::unique_ptr<pullstat>
  Create(const util::filepath & sourceFile)
  {
    return std::make_unique<pullstat>(sourceFile);
  }
};

static bool
empty(const rvsdg::GammaNode * gamma)
{
  for (size_t n = 0; n < gamma->nsubregions(); n++)
  {
    if (gamma->subregion(n)->nnodes() != 0)
      return false;
  }

  return true;
}

static bool
single_successor(const jlm::rvsdg::node * node)
{
  std::unordered_set<jlm::rvsdg::node *> successors;
  for (size_t n = 0; n < node->noutputs(); n++)
  {
    for (const auto & user : *node->output(n))
      successors.insert(rvsdg::input::GetNode(*user));
  }

  return successors.size() == 1;
}

static void
remove(rvsdg::GammaInput * input)
{
  auto gamma = input->node();

  for (size_t n = 0; n < gamma->nsubregions(); n++)
    gamma->subregion(n)->RemoveArgument(input->index() - 1);
  gamma->RemoveInput(input->index());
}

static void
pullin_node(rvsdg::GammaNode * gamma, jlm::rvsdg::node * node)
{
  /* collect operands */
  std::vector<std::vector<jlm::rvsdg::output *>> operands(gamma->nsubregions());
  for (size_t i = 0; i < node->ninputs(); i++)
  {
    auto ev = gamma->add_entryvar(node->input(i)->origin());
    for (size_t a = 0; a < ev->narguments(); a++)
      operands[a].push_back(ev->argument(a));
  }

  /* copy node into subregions */
  for (size_t r = 0; r < gamma->nsubregions(); r++)
  {
    auto copy = node->copy(gamma->subregion(r), operands[r]);

    /* redirect outputs */
    for (size_t o = 0; o < node->noutputs(); o++)
    {
      for (const auto & user : *node->output(o))
      {
        JLM_ASSERT(dynamic_cast<jlm::rvsdg::structural_input *>(user));
        auto sinput = static_cast<jlm::rvsdg::structural_input *>(user);
        auto argument = gamma->subregion(r)->argument(sinput->index() - 1);
        argument->divert_users(copy->output(o));
      }
    }
  }
}

static void
cleanup(rvsdg::GammaNode * gamma, jlm::rvsdg::node * node)
{
  JLM_ASSERT(single_successor(node));

  /* remove entry variables and node */
  for (size_t n = 0; n < node->noutputs(); n++)
  {
    while (node->output(n)->nusers() != 0)
      remove(util::AssertedCast<rvsdg::GammaInput>(*node->output(n)->begin()));
  }
  remove(node);
}

void
pullin_top(rvsdg::GammaNode * gamma)
{
  /* FIXME: This is inefficient. We can do better. */
  auto ev = gamma->begin_entryvar();
  while (ev != gamma->end_entryvar())
  {
    auto node = jlm::rvsdg::output::GetNode(*ev->origin());
    auto tmp = jlm::rvsdg::output::GetNode(*gamma->predicate()->origin());
    if (node && tmp != node && single_successor(node))
    {
      pullin_node(gamma, node);

      cleanup(gamma, node);

      ev = gamma->begin_entryvar();
    }
    else
    {
      ev++;
    }
  }
}

void
pullin_bottom(rvsdg::GammaNode * gamma)
{
  /* collect immediate successors of the gamma node */
  std::unordered_set<jlm::rvsdg::node *> workset;
  for (size_t n = 0; n < gamma->noutputs(); n++)
  {
    auto output = gamma->output(n);
    for (const auto & user : *output)
    {
      auto node = rvsdg::input::GetNode(*user);
      if (node && node->depth() == gamma->depth() + 1)
        workset.insert(node);
    }
  }

  while (!workset.empty())
  {
    auto node = *workset.begin();
    workset.erase(node);

    /* copy node into subregions */
    std::vector<std::vector<jlm::rvsdg::output *>> outputs(node->noutputs());
    for (size_t r = 0; r < gamma->nsubregions(); r++)
    {
      /* collect operands */
      std::vector<jlm::rvsdg::output *> operands;
      for (size_t i = 0; i < node->ninputs(); i++)
      {
        auto input = node->input(i);
        if (jlm::rvsdg::output::GetNode(*input->origin()) == gamma)
        {
          auto output = static_cast<jlm::rvsdg::structural_output *>(input->origin());
          operands.push_back(gamma->subregion(r)->result(output->index())->origin());
        }
        else
        {
          auto ev = gamma->add_entryvar(input->origin());
          operands.push_back(ev->argument(r));
        }
      }

      auto copy = node->copy(gamma->subregion(r), operands);
      for (size_t o = 0; o < copy->noutputs(); o++)
        outputs[o].push_back(copy->output(o));
    }

    /* adjust outputs and update workset */
    for (size_t n = 0; n < node->noutputs(); n++)
    {
      auto output = node->output(n);
      for (const auto & user : *output)
      {
        auto tmp = rvsdg::input::GetNode(*user);
        if (tmp && tmp->depth() == node->depth() + 1)
          workset.insert(tmp);
      }

      auto xv = gamma->add_exitvar(outputs[n]);
      output->divert_users(xv);
    }
  }
}

static size_t
is_used_in_nsubregions(const rvsdg::GammaNode * gamma, const jlm::rvsdg::node * node)
{
  JLM_ASSERT(single_successor(node));

  /* collect all gamma inputs */
  std::unordered_set<const rvsdg::GammaInput *> inputs;
  for (size_t n = 0; n < node->noutputs(); n++)
  {
    for (const auto & user : *(node->output(n)))
    {
      inputs.insert(util::AssertedCast<const rvsdg::GammaInput>(user));
    }
  }

  /* collect subregions where node is used */
  std::unordered_set<rvsdg::Region *> subregions;
  for (const auto & input : inputs)
  {
    for (const auto & argument : *input)
    {
      if (argument.nusers() != 0)
        subregions.insert(argument.region());
    }
  }

  return subregions.size();
}

void
pull(rvsdg::GammaNode * gamma)
{
  /*
    We don't want to pull anything into empty gammas with two subregions,
    as they are translated to select instructions in the r2j phase.
  */
  if (gamma->nsubregions() == 2 && empty(gamma))
    return;

  auto prednode = jlm::rvsdg::output::GetNode(*gamma->predicate()->origin());

  /* FIXME: This is inefficient. We can do better. */
  auto ev = gamma->begin_entryvar();
  while (ev != gamma->end_entryvar())
  {
    auto node = jlm::rvsdg::output::GetNode(*ev->origin());
    if (!node || prednode == node || !single_successor(node))
    {
      ev++;
      continue;
    }

    if (is_used_in_nsubregions(gamma, node) == 1)
    {
      /*
        FIXME: This function pulls in the node to ALL subregions and
        not just the one we care about.
      */
      pullin_node(gamma, node);
      cleanup(gamma, node);
      ev = gamma->begin_entryvar();
    }
    else
    {
      ev++;
    }
  }
}

void
pull(rvsdg::Region * region)
{
  for (auto & node : jlm::rvsdg::topdown_traverser(region))
  {
    if (auto structnode = dynamic_cast<jlm::rvsdg::structural_node *>(node))
    {
      if (auto gamma = dynamic_cast<rvsdg::GammaNode *>(node))
        pull(gamma);

      for (size_t n = 0; n < structnode->nsubregions(); n++)
        pull(structnode->subregion(n));
    }
  }
}

static void
pull(RvsdgModule & rm, util::StatisticsCollector & statisticsCollector)
{
  auto statistics = pullstat::Create(rm.SourceFileName());

  statistics->start(rm.Rvsdg());
  pull(rm.Rvsdg().root());
  statistics->end(rm.Rvsdg());

  statisticsCollector.CollectDemandedStatistics(std::move(statistics));
}

/* pullin class */

pullin::~pullin()
{}

void
pullin::run(RvsdgModule & module, util::StatisticsCollector & statisticsCollector)
{
  pull(module, statisticsCollector);
}

}

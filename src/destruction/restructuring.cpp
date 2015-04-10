/*
 * Copyright 2015 Nico Reißmann <nico.reissmann@gmail.com>
 * See COPYING for terms of redistribution.
 */

#include <jlm/IR/basic_block.hpp>
#include <jlm/IR/cfg.hpp>
#include <jlm/IR/cfg_node.hpp>

#include <jive/types/bitstring/constant.h>
#include <jive/vsdg/controltype.h>
#include <jive/vsdg/operators/match.h>

#include <cmath>
#include <deque>
#include <unordered_map>
#include <unordered_set>

namespace jlm {

static void
find_entries_and_exits(
	const std::unordered_set<jlm::cfg_node*> & scc,
	std::unordered_set<jlm::cfg_edge*> & ae,
	std::unordered_map<jlm::cfg_node*, size_t> & ve,
	std::unordered_set<jlm::cfg_edge*> & ax,
	std::unordered_map<jlm::cfg_node*, size_t> & vx,
	std::unordered_set<jlm::cfg_edge*> & ar)
{
	for (auto node : scc) {
		std::list<jlm::cfg_edge*> inedges = node->inedges();
		for (auto edge : inedges) {
			if (scc.find(edge->source()) == scc.end()) {
				ae.insert(edge);
				if (ve.find(node) == ve.end())
					ve.insert(std::make_pair(node, ve.size()));
			}
		}

		std::vector<jlm::cfg_edge*> outedges = node->outedges();
		for (auto edge : outedges) {
			if (scc.find(edge->sink()) == scc.end()) {
				ax.insert(edge);
				if (vx.find(edge->sink()) == vx.end())
					vx.insert(std::make_pair(edge->sink(), vx.size()));
			}
		}
	}

	for (auto node : scc) {
		std::vector<jlm::cfg_edge*> outedges = node->outedges();
		for (auto edge : outedges) {
			if (ve.find(edge->sink()) != ve.end())
				ar.insert(edge);
		}
	}
}

static void
restructure_loops(jlm::cfg_node * entry, jlm::cfg_node * exit,
	std::vector<jlm::cfg_edge> & back_edges)
{
	jlm::cfg * cfg = entry->cfg();

	std::vector<std::unordered_set<jlm::cfg_node*>> sccs = cfg->find_sccs();

	for (auto scc : sccs) {
		std::unordered_set<jlm::cfg_edge*> ae;
		std::unordered_map<jlm::cfg_node*, size_t> ve;
		std::unordered_set<jlm::cfg_edge*> ax;
		std::unordered_map<jlm::cfg_node*, size_t> vx;
		std::unordered_set<jlm::cfg_edge*> ar;
		find_entries_and_exits(scc, ae, ve, ax, vx, ar);

		/* The loop already has the required structure, nothing needs to be inserted */
		if (ae.size() == 1 && ar.size() == 1 && ax.size() == 1
			&& (*ar.begin())->source() == (*ax.begin())->source())
		{
			jlm::cfg_edge * r = *ar.begin();
			back_edges.push_back(jlm::cfg_edge(r->source(), r->sink(), r->index()));
			r->source()->remove_outedge(r);
			restructure_loops((*ae.begin())->sink(), (*ax.begin())->source(), back_edges);
			continue;
		}

		/* Restructure loop */
		size_t nbits = std::max(std::ceil(std::log2(std::max(ve.size(), vx.size()))), 1.0);
		const variable * q = cfg->create_variable(jive::bits::type(nbits), "#q#");

		const variable * r = cfg->create_variable(jive::bits::type(1), "#r#");
		jlm::basic_block * vt = cfg->create_basic_block();
		vt->append(jive::match_op(1, {{0, 0}}, 1, 2), {r});


		/* handle loop entries */
		jlm::basic_block * new_ve;
		if (ve.size() > 1) {
			new_ve = cfg->create_basic_block();

			std::map<uint64_t, uint64_t> ve_mapping;
			for (size_t n = 0; n < ve.size()-1; n++)
				ve_mapping[n] = n;
			new_ve->append(jive::match_op(nbits, ve_mapping, ve.size()-1, ve.size()), {q});

			for (auto edge : ae) {
				jlm::basic_block * ass = cfg->create_basic_block();
				jive::bits::constant_op op(jive::bits::value_repr(nbits, ve[edge->sink()]));
				ass->append(op, {}, {q});
				ass->add_outedge(new_ve, 0);
				edge->divert(ass);
			}

			for (auto v : ve)
				new_ve->add_outedge(v.first, v.second);
		} else
			new_ve = static_cast<basic_block*>(ve.begin()->first);


		/* handle loop exists */
		basic_block * new_vx;
		if (vx.size() > 1) {
			new_vx = cfg->create_basic_block();

			std::map<uint64_t, uint64_t> vx_mapping;
			for (size_t n = 0; n < vx.size()-1; n++)
				vx_mapping[n] = n;
			new_vx->append(jive::match_op(nbits, vx_mapping, vx.size()-1, vx.size()), {q});

			for (auto v : vx)
				new_vx->add_outedge(v.first, v.second);
		} else
			new_vx = static_cast<basic_block*>(vx.begin()->first);

		for (auto edge : ax) {
			jlm::basic_block * ass = cfg->create_basic_block();
			ass->append(jive::bits::constant_op(jive::bits::value_repr(1, 0)), {}, {r});
			if (vx.size() > 1) {
				jive::bits::constant_op op(jive::bits::value_repr(nbits, vx[edge->sink()]));
				ass->append(op, {}, {q});
			}
			ass->add_outedge(vt, 0);
			edge->divert(ass);
		}


		/* handle loop repetition */
		for (auto edge : ar) {
			jlm::basic_block * ass = cfg->create_basic_block();
			ass->append(jive::bits::constant_op(jive::bits::value_repr(1, 1)), {}, {r});
			if (ve.size() > 1) {
				jive::bits::constant_op op(jive::bits::value_repr(nbits, ve[edge->sink()]));
				ass->append(op, {}, {q});
			}
			ass->add_outedge(vt, 0);
			edge->divert(ass);
		}

		vt->add_outedge(new_vx, 0);
		back_edges.push_back(jlm::cfg_edge(vt, new_ve, 1));

		restructure_loops(new_ve, vt, back_edges);
	}
}

static const jlm::cfg_node *
find_head_branch(const jlm::cfg_node * start, const jlm::cfg_node * end)
{
	do {
		if (start->is_branch() || start == end)
			break;

		start = start->outedges()[0]->sink();
	} while (1);

	return start;
}

static std::unordered_set<jlm::cfg_node*>
find_dominator_graph(const jlm::cfg_edge * edge)
{
	std::unordered_set<jlm::cfg_node*> nodes;
	std::unordered_set<const jlm::cfg_edge*> edges({edge});

	std::deque<jlm::cfg_node*> to_visit(1, edge->sink());
	while (to_visit.size() != 0) {
		jlm::cfg_node * node = to_visit.front(); to_visit.pop_front();
		if (nodes.find(node) != nodes.end())
			continue;

		bool accept = true;
		std::list<jlm::cfg_edge*> inedges = node->inedges();
		for (auto e : inedges) {
			if (edges.find(e) == edges.end()) {
				accept = false;
				break;
			}
		}

		if (accept) {
			nodes.insert(node);
			std::vector<jlm::cfg_edge*> outedges = node->outedges();
			for (auto e : outedges) {
				edges.insert(e);
				to_visit.push_back(e->sink());
			}
		}
	}

	return nodes;
}

static void
restructure_branches(jlm::cfg_node * start, jlm::cfg_node * end)
{
	jlm::cfg * cfg = start->cfg();

	const jlm::cfg_node * head_branch = find_head_branch(start, end);
	if (head_branch == end)
		return;

	/* Compute the branch graphs and insert their nodes into sets. */
	std::unordered_set<jlm::cfg_node*> all_branch_nodes;
	std::vector<std::unordered_set<jlm::cfg_node*>> branch_nodes;
	std::vector<jlm::cfg_edge*> af = head_branch->outedges();
	for (size_t n = 0; n < af.size(); n++) {
		std::unordered_set<jlm::cfg_node*> branch = find_dominator_graph(af[n]);
		branch_nodes.push_back(std::unordered_set<jlm::cfg_node*>(branch.begin(),
			branch.end()));
		all_branch_nodes.insert(branch.begin(), branch.end());
	}

	/* Compute continuation points and the branch out edges. */
	std::unordered_map<jlm::cfg_node*, size_t> cpoints;
	std::vector<std::unordered_set<jlm::cfg_edge*>> branch_out_edges;
	for (size_t n = 0; n < branch_nodes.size(); n++) {
		branch_out_edges.push_back(std::unordered_set<jlm::cfg_edge*>());
		if (branch_nodes[n].empty()) {
			branch_out_edges[n].insert(af[n]);
			cpoints.insert(std::make_pair(af[n]->sink(), cpoints.size()));
		} else {
			for (auto node : branch_nodes[n]) {
				std::vector<jlm::cfg_edge*> outedges = node->outedges();
				for (auto edge : outedges) {
					if (all_branch_nodes.find(edge->sink()) == all_branch_nodes.end()) {
						branch_out_edges[n].insert(edge);
						cpoints.insert(std::make_pair(edge->sink(), cpoints.size()));
					}
				}
			}
		}
	}
	JLM_DEBUG_ASSERT(!cpoints.empty());

	/* Nothing needs to be restructured for just one continuation point */
	if (cpoints.size() == 1) {
		jlm::cfg_node * cpoint = cpoints.begin()->first;
		JLM_DEBUG_ASSERT(branch_out_edges.size() == af.size());
		for (size_t n = 0; n < af.size(); n++) {
			/* empty branch subgraph, nothing needs to be done */
			if (af[n]->sink() == cpoint)
				continue;

			/* only one branch out edge leads to the continuation point */
			if (branch_out_edges[n].size() == 1) {
				restructure_branches(af[n]->sink(), (*branch_out_edges[n].begin())->source());
				continue;
			}

			/* more than one branch out edge leads to the continuation point */
			jlm::basic_block * null = cfg->create_basic_block();
			null->add_outedge(cpoints.begin()->first, 0);
			for (auto edge : branch_out_edges[n])
				edge->divert(null);
			restructure_branches(af[n]->sink(), null);
		}

		/* restructure tail subgraph */
		restructure_branches(cpoint, end);
		return;
	}

	/* Insert vt into CFG and add outgoing edges to the continuation points */
	size_t nbits = std::ceil(std::log2(cpoints.size()));
	const variable * p = cfg->create_variable(jive::bits::type(nbits), "#p#");
	jlm::basic_block * vt = cfg->create_basic_block();
	std::map<uint64_t, uint64_t> mapping;
	for (size_t n = 0; n < cpoints.size()-1; n++)
		mapping[n] = n;
	vt->append(jive::match_op(nbits, mapping, cpoints.size()-1, cpoints.size()), {p});
	for (auto it = cpoints.begin(); it != cpoints.end(); it++)
		vt->add_outedge(it->first, it->second);

	JLM_DEBUG_ASSERT(branch_out_edges.size() == af.size());
	for (size_t n = 0; n < af.size(); n++) {
		/* one branch out edge for this branch subgraph, only add auxiliary assignment */
		if (branch_out_edges[n].size() == 1) {
			cfg_edge * boe = *branch_out_edges[n].begin();
			jlm::basic_block * ass = cfg->create_basic_block();
			jive::bits::constant_op op(jive::bits::value_repr(nbits, cpoints[boe->sink()]));
			ass->append(op, {}, {p});
			ass->add_outedge(vt, 0);
			boe->divert(ass);
			/* if the branch subgraph is not empty, we need to restructure it */
			if (boe != af[n])
				restructure_branches(af[n]->sink(), ass);
			continue;
		}

		/* more than one branch out edge */
		jlm::basic_block * null = cfg->create_basic_block();
		null->add_outedge(vt, 0);
		for (auto edge : branch_out_edges[n]) {
			jlm::basic_block * ass = cfg->create_basic_block();
			jive::bits::constant_op op(jive::bits::value_repr(nbits, cpoints[edge->sink()]));
			ass->append(op, {}, {p});
			ass->add_outedge(null, 0);
			edge->divert(ass);
		}
		restructure_branches(af[n]->sink(), null);
	}

	/* restructure tail subgraph */
	restructure_branches(vt, end);
}

std::unordered_set<const jlm::cfg_edge*>
restructure(jlm::cfg * cfg)
{
	JLM_DEBUG_ASSERT(cfg->is_closed());

	std::vector<jlm::cfg_edge> back_edges;
	restructure_loops(cfg->enter(), cfg->exit(), back_edges);

	JLM_DEBUG_ASSERT(cfg->is_acyclic());

	restructure_branches(cfg->enter(), cfg->exit());

	/* insert back edges */
	std::unordered_set<const jlm::cfg_edge*> edges;
	for (auto edge : back_edges)
		edges.insert(edge.source()->add_outedge(edge.sink(), edge.index()));

	JLM_DEBUG_ASSERT(cfg->is_structured());
	return edges;
}

}

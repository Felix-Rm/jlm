/*
 * Copyright 2021 Nico Reißmann <nico.reissmann@gmail.com>
 * See COPYING for terms of redistribution.
 */

#ifndef JLM_OPT_ALIAS_ANALYSES_ENCODERS_HPP
#define JLM_OPT_ALIAS_ANALYSES_ENCODERS_HPP

#include <memory>

namespace jive {

class gamma_node;
class region;
class simple_node;
class structural_node;
class theta_node;

namespace phi { class node; }

}

namespace jlm {

class rvsdg_module;

namespace delta { class node; }
namespace lambda { class node; }

namespace aa {

class ptg;

/** \brief Encodes a Points-To graph in the RVSDG.
*/
class MemoryStateEncoder {
public:
	virtual
	~MemoryStateEncoder();

	virtual void
	Encode(rvsdg_module & module) = 0;

	virtual void
	EncodeAlloca(const jive::simple_node & node) = 0;

	virtual void
	EncodeMalloc(const jive::simple_node & node) = 0;

	virtual void
	EncodeLoad(const jive::simple_node & node) = 0;

	virtual void
	EncodeStore(const jive::simple_node & node) = 0;

	virtual void
	EncodeFree(const jive::simple_node & node) = 0;

	virtual void
	EncodeCall(const jive::simple_node & node) = 0;

	virtual void
	EncodeMemcpy(const jive::simple_node & node) = 0;

	virtual void
	Encode(const lambda::node & lambda) = 0;

	virtual void
	Encode(const jive::phi::node & phi) = 0;

	virtual void
	Encode(const delta::node & delta) = 0;

	virtual void
	Encode(jive::gamma_node & gamma) = 0;

	virtual void
	Encode(jive::theta_node & theta) = 0;

protected:
	void
	Encode(jive::region & region);

	void
	Encode(jive::structural_node & node);

	void
	Encode(const jive::simple_node & node);
};

}}

#endif

/*
 * Copyright 2017 Nico Reißmann <nico.reissmann@gmail.com>
 * See COPYING for terms of redistribution.
 */

#ifndef JLM_IR_RVSDGMODULE_HPP
#define JLM_IR_RVSDGMODULE_HPP

#include <jive/rvsdg/graph.hpp>

#include <jlm/ir/linkage.hpp>
#include <jlm/util/file.hpp>

namespace jlm {

/* impport class */

class impport final : public jive::impport {
public:
	virtual
	~impport();

	impport(
		const jive::type & type
	, const std::string & name
	, const jlm::linkage & lnk)
	: jive::impport(type, name)
	, linkage_(lnk)
	{}

	impport(const impport & other)
	: jive::impport(other)
	, linkage_(other.linkage_)
	{}

	impport(impport && other)
	: jive::impport(other)
	, linkage_(std::move(other.linkage_))
	{}

	impport&
	operator=(const impport&) = delete;

	impport&
	operator=(impport&&) = delete;

	const jlm::linkage &
	linkage() const noexcept
	{
		return linkage_;
	}

	virtual bool
	operator==(const port&) const noexcept override;

	virtual std::unique_ptr<port>
	copy() const override;

private:
	jlm::linkage linkage_;
};

static inline bool
is_import(const jive::output * output)
{
	auto graph = output->region()->graph();

	auto argument = dynamic_cast<const jive::argument*>(output);
	return argument && argument->region() == graph->root();
}

static inline bool
is_export(const jive::input * input)
{
	auto graph = input->region()->graph();

	auto result = dynamic_cast<const jive::result*>(input);
	return result && result->region() == graph->root();
}

/** \brief RVSDG module class
 *
 */
class RvsdgModule final {
public:
	inline
	RvsdgModule(
		const jlm::filepath & source_filename,
		const std::string & target_triple,
		const std::string & data_layout)
	: data_layout_(data_layout)
	, target_triple_(target_triple)
	, source_filename_(source_filename)
	{}

	RvsdgModule(const RvsdgModule &) = delete;

	RvsdgModule(RvsdgModule &&) = delete;

	RvsdgModule &
	operator=(const RvsdgModule &) = delete;

	RvsdgModule &
	operator=(RvsdgModule &&) = delete;

	inline jive::graph *
	graph() noexcept
	{
		return &graph_;
	}

	inline const jive::graph *
	graph() const noexcept
	{
		return &graph_;
	}

	const jlm::filepath &
	source_filename() const noexcept
	{
		return source_filename_;
	}

	inline const std::string &
	target_triple() const noexcept
	{
		return target_triple_;
	}

	inline const std::string &
	data_layout() const noexcept
	{
		return data_layout_;
	}

	static std::unique_ptr<RvsdgModule>
	create(
		const jlm::filepath & source_filename,
		const std::string & target_triple,
		const std::string & data_layout)
	{
		return std::make_unique<RvsdgModule>(source_filename, target_triple, data_layout);
	}

private:
	jive::graph graph_;
	std::string data_layout_;
	std::string target_triple_;
	const jlm::filepath source_filename_;
};

}

#endif
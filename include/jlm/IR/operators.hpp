/*
 * Copyright 2014 2015 Nico Reißmann <nico.reissmann@gmail.com>
 * See COPYING for terms of redistribution.
 */

#ifndef JLM_IR_OPERATORS_H
#define JLM_IR_OPERATORS_H

#include <jive/types/bitstring/type.h>
#include <jive/types/function/fcttype.h>
#include <jive/vsdg/basetype.h>
#include <jive/vsdg/operators/nullary.h>

#include <jlm/IR/tac.hpp>

namespace jlm {

/* phi operator */

class phi_op final : public jive::simple_op {
public:
	virtual
	~phi_op() noexcept;

	/* FIXME: check that number of arguments is not zero */
	inline
	phi_op(size_t narguments, const jive::base::type & type)
	: narguments_(narguments)
	, type_(type.copy())
	{}

	inline
	phi_op(const phi_op & other)
	: narguments_(other.narguments_)
	, type_(other.type_->copy())
	{}

	inline
	phi_op(phi_op && other) = default;

	virtual bool
	operator==(const operation & other) const noexcept override;

	virtual size_t
	narguments() const noexcept override;

	virtual const jive::base::type &
	argument_type(size_t index) const noexcept override;

	virtual size_t
	nresults() const noexcept override;

	virtual const jive::base::type &
	result_type(size_t index) const noexcept override;

	virtual std::string
	debug_string() const override;

	virtual std::unique_ptr<jive::operation>
	copy() const override;

	inline const jive::base::type &
	type() const noexcept
	{
		return *type_;
	}

private:
	size_t narguments_;
	std::unique_ptr<jive::base::type> type_;
};

static inline std::unique_ptr<jlm::tac>
create_phi_tac(const std::vector<const variable*> & arguments, const variable * result)
{
	phi_op phi(arguments.size(), result->type());
	return create_tac(phi, arguments, {result});
}

/* assignment operator */

class assignment_op final : public jive::simple_op {
public:
	virtual
	~assignment_op() noexcept;

	inline
	assignment_op(const jive::base::type & type)
	: type_(type.copy())
	{}

	inline
	assignment_op(const assignment_op & other)
	: type_(other.type_->copy())
	{}

	inline
	assignment_op(assignment_op && other) = default;

	virtual bool
	operator==(const operation & other) const noexcept override;

	virtual size_t
	narguments() const noexcept override;

	virtual const jive::base::type &
	argument_type(size_t index) const noexcept override;

	virtual size_t
	nresults() const noexcept override;

	virtual const jive::base::type &
	result_type(size_t index) const noexcept override;

	virtual std::string
	debug_string() const override;

	virtual std::unique_ptr<jive::operation>
	copy() const override;

private:
	std::unique_ptr<jive::base::type> type_;
};

static inline std::unique_ptr<jlm::tac>
create_assignment(
	const jive::base::type & type,
	const variable * arg,
	const variable * r)
{
	return create_tac(assignment_op(type), {arg}, {r});
}

static inline bool
is_assignment_op(const jive::operation & op)
{
	return dynamic_cast<const assignment_op*>(&op) != nullptr;
}

/* select operator */

class select_op final : public jive::simple_op {
public:
	virtual
	~select_op() noexcept;

	inline
	select_op(const jive::base::type & type)
	: type_(type.copy())
	{}

	inline
	select_op(const select_op & other)
	: type_(other.type_->copy())
	{}

	inline
	select_op(select_op && other) = default;

	virtual bool
	operator==(const operation & other) const noexcept override;

	virtual size_t
	narguments() const noexcept override;

	virtual const jive::base::type &
	argument_type(size_t index) const noexcept override;

	virtual size_t
	nresults() const noexcept override;

	virtual const jive::base::type &
	result_type(size_t index) const noexcept override;

	virtual std::string
	debug_string() const override;

	virtual std::unique_ptr<jive::operation>
	copy() const override;

	inline const jive::base::type &
	type() const noexcept
	{
		return *type_;
	}

private:
	std::unique_ptr<jive::base::type> type_;
};

static inline bool
is_select_op(const jive::operation & op)
{
	return dynamic_cast<const select_op*>(&op) != nullptr;
}

/* alloca operator */

class alloca_op final : public jive::simple_op {
public:
	virtual
	~alloca_op() noexcept;

	inline
	alloca_op(size_t nbytes)
		: nbytes_(nbytes)
	{}

	inline
	alloca_op(const alloca_op & other) = default;

	inline
	alloca_op(alloca_op && other) = default;

	virtual bool
	operator==(const operation & other) const noexcept override;

	virtual size_t
	narguments() const noexcept override;

	virtual const jive::base::type &
	argument_type(size_t index) const noexcept override;

	virtual size_t
	nresults() const noexcept override;

	virtual const jive::base::type &
	result_type(size_t index) const noexcept override;

	virtual std::string
	debug_string() const override;

	virtual std::unique_ptr<jive::operation>
	copy() const override;

private:
	size_t nbytes_;
};

/* bits2flt operator */

class bits2flt_op final : public jive::simple_op {
public:
	virtual
	~bits2flt_op() noexcept;

	inline
	bits2flt_op(const jive::bits::type & type)
		: itype_(type)
	{}

	inline
	bits2flt_op(const bits2flt_op & other) = default;

	inline
	bits2flt_op(bits2flt_op && other) = default;

	virtual bool
	operator==(const operation & other) const noexcept override;

	virtual size_t
	narguments() const noexcept override;

	virtual const jive::base::type &
	argument_type(size_t index) const noexcept override;

	virtual size_t
	nresults() const noexcept override;

	virtual const jive::base::type &
	result_type(size_t index) const noexcept override;

	virtual std::string
	debug_string() const override;

	virtual std::unique_ptr<jive::operation>
	copy() const override;

private:
	jive::bits::type itype_;
};

/* flt2bits operator */

class flt2bits_op final : public jive::simple_op {
public:
	virtual
	~flt2bits_op() noexcept;

	inline
	flt2bits_op(const jive::bits::type & type)
		: otype_(type)
	{}

	inline
	flt2bits_op(const flt2bits_op & other) = default;

	inline
	flt2bits_op(flt2bits_op && other) = default;

	virtual bool
	operator==(const operation & other) const noexcept override;

	virtual size_t
	narguments() const noexcept override;

	virtual const jive::base::type &
	argument_type(size_t index) const noexcept override;

	virtual size_t
	nresults() const noexcept override;

	virtual const jive::base::type &
	result_type(size_t index) const noexcept override;

	virtual std::string
	debug_string() const override;

	virtual std::unique_ptr<jive::operation>
	copy() const override;

private:
	jive::bits::type otype_;
};

}

#endif

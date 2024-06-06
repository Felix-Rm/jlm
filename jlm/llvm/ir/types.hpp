/*
 * Copyright 2017 Nico Reißmann <nico.reissmann@gmail.com>
 * See COPYING for terms of redistribution.
 */

#ifndef JLM_LLVM_IR_TYPES_HPP
#define JLM_LLVM_IR_TYPES_HPP

#include <jlm/rvsdg/type.hpp>
#include <jlm/util/common.hpp>
#include <jlm/util/iterator_range.hpp>

#include <memory>
#include <vector>

namespace jlm::llvm
{

/** \brief Function type class
 *
 */
class FunctionType final : public jlm::rvsdg::valuetype
{
public:
  ~FunctionType() noexcept override;

  FunctionType(
      std::vector<std::shared_ptr<const jlm::rvsdg::type>> argumentTypes,
      std::vector<std::shared_ptr<const jlm::rvsdg::type>> resultTypes);

  FunctionType(const FunctionType & other);

  FunctionType(FunctionType && other) noexcept;

  FunctionType &
  operator=(const FunctionType & other);

  FunctionType &
  operator=(FunctionType && other) noexcept;

  const std::vector<std::shared_ptr<const jlm::rvsdg::type>> &
  Arguments() const noexcept;

  const std::vector<std::shared_ptr<const jlm::rvsdg::type>> &
  Results() const noexcept;

  size_t
  NumResults() const noexcept
  {
    return ResultTypes_.size();
  }

  size_t
  NumArguments() const noexcept
  {
    return ArgumentTypes_.size();
  }

  const jlm::rvsdg::type &
  ResultType(size_t index) const noexcept
  {
    JLM_ASSERT(index < ResultTypes_.size());
    return *ResultTypes_[index];
  }

  const jlm::rvsdg::type &
  ArgumentType(size_t index) const noexcept
  {
    JLM_ASSERT(index < ArgumentTypes_.size());
    return *ArgumentTypes_[index];
  }

  std::string
  debug_string() const override;

  bool
  operator==(const jlm::rvsdg::type & other) const noexcept override;

  std::shared_ptr<const jlm::rvsdg::type>
  copy() const override;

  static std::shared_ptr<const FunctionType>
  Create(
      std::vector<std::shared_ptr<const jlm::rvsdg::type>> argumentTypes,
      std::vector<std::shared_ptr<const jlm::rvsdg::type>> resultTypes);

private:
  std::vector<std::shared_ptr<const jlm::rvsdg::type>> ResultTypes_;
  std::vector<std::shared_ptr<const jlm::rvsdg::type>> ArgumentTypes_;
};

/** \brief PointerType class
 *
 * This operator is the Jlm equivalent of LLVM's PointerType class.
 */
class PointerType final : public jlm::rvsdg::valuetype
{
public:
  ~PointerType() noexcept override;

  PointerType() = default;

  [[nodiscard]] std::string
  debug_string() const override;

  bool
  operator==(const jlm::rvsdg::type & other) const noexcept override;

  [[nodiscard]] std::shared_ptr<const jlm::rvsdg::type>
  copy() const override;

  static std::shared_ptr<const PointerType>
  Create();
};

/* array type */

class arraytype final : public jlm::rvsdg::valuetype
{
public:
  virtual ~arraytype();

  inline arraytype(std::shared_ptr<const jlm::rvsdg::valuetype> type, size_t nelements)
      : jlm::rvsdg::valuetype(),
        nelements_(nelements),
        type_(std::move(type))
  {}

  inline arraytype(const arraytype & other) = default;

  inline arraytype(arraytype && other) = default;

  inline arraytype &
  operator=(const arraytype &) = delete;

  inline arraytype &
  operator=(arraytype &&) = delete;

  virtual std::string
  debug_string() const override;

  virtual bool
  operator==(const jlm::rvsdg::type & other) const noexcept override;

  std::shared_ptr<const jlm::rvsdg::type>
  copy() const override;

  inline size_t
  nelements() const noexcept
  {
    return nelements_;
  }

  inline const jlm::rvsdg::valuetype &
  element_type() const noexcept
  {
    return *static_cast<const jlm::rvsdg::valuetype *>(type_.get());
  }

  static std::shared_ptr<const arraytype>
  Create(std::shared_ptr<const jlm::rvsdg::valuetype> type, size_t nelements)
  {
    return std::make_shared<arraytype>(std::move(type), nelements);
  }

private:
  size_t nelements_;
  std::shared_ptr<const jlm::rvsdg::type> type_;
};

/* floating point type */

enum class fpsize
{
  half,
  flt,
  dbl,
  x86fp80
};

class fptype final : public jlm::rvsdg::valuetype
{
public:
  virtual ~fptype();

  inline fptype(const fpsize & size)
      : jlm::rvsdg::valuetype(),
        size_(size)
  {}

  virtual std::string
  debug_string() const override;

  virtual bool
  operator==(const jlm::rvsdg::type & other) const noexcept override;

  std::shared_ptr<const jlm::rvsdg::type>
  copy() const override;

  inline const fpsize &
  size() const noexcept
  {
    return size_;
  }

  static std::shared_ptr<const fptype>
  Create(fpsize size);

private:
  fpsize size_;
};

/* vararg type */

class varargtype final : public jlm::rvsdg::statetype
{
public:
  virtual ~varargtype();

  inline constexpr varargtype()
      : jlm::rvsdg::statetype()
  {}

  virtual bool
  operator==(const jlm::rvsdg::type & other) const noexcept override;

  std::shared_ptr<const jlm::rvsdg::type>
  copy() const override;

  virtual std::string
  debug_string() const override;

  static std::shared_ptr<const varargtype>
  Create();
};

static inline bool
is_varargtype(const jlm::rvsdg::type & type)
{
  return dynamic_cast<const varargtype *>(&type) != nullptr;
}

static inline std::unique_ptr<jlm::rvsdg::type>
create_varargtype()
{
  return std::unique_ptr<jlm::rvsdg::type>(new varargtype());
}

/** \brief StructType class
 *
 * This class is the equivalent of LLVM's StructType class.
 */
class StructType final : public jlm::rvsdg::valuetype
{
public:
  class Declaration;

  ~StructType() override;

  StructType(bool isPacked, const Declaration & declaration)
      : jlm::rvsdg::valuetype(),
        IsPacked_(isPacked),
        Declaration_(declaration)
  {}

  StructType(std::string name, bool isPacked, const Declaration & declaration)
      : jlm::rvsdg::valuetype(),
        IsPacked_(isPacked),
        Name_(std::move(name)),
        Declaration_(declaration)
  {}

  StructType(const StructType &) = default;

  StructType(StructType &&) = delete;

  StructType &
  operator=(const StructType &) = delete;

  StructType &
  operator=(StructType &&) = delete;

  [[nodiscard]] bool
  HasName() const noexcept
  {
    return !Name_.empty();
  }

  [[nodiscard]] const std::string &
  GetName() const noexcept
  {
    return Name_;
  }

  [[nodiscard]] bool
  IsPacked() const noexcept
  {
    return IsPacked_;
  }

  [[nodiscard]] const Declaration &
  GetDeclaration() const noexcept
  {
    return Declaration_;
  }

  bool
  operator==(const jlm::rvsdg::type & other) const noexcept override;

  [[nodiscard]] std::shared_ptr<const jlm::rvsdg::type>
  copy() const override;

  [[nodiscard]] std::string
  debug_string() const override;

  static std::shared_ptr<const StructType>
  Create(const std::string & name, bool isPacked, const Declaration & declaration)
  {
    return std::make_shared<StructType>(name, isPacked, declaration);
  }

  static std::shared_ptr<const StructType>
  Create(bool isPacked, const Declaration & declaration)
  {
    return std::make_shared<StructType>(isPacked, declaration);
  }

private:
  bool IsPacked_;
  std::string Name_;
  const Declaration & Declaration_;
};

class StructType::Declaration final
{
public:
  ~Declaration() = default;

  Declaration(std::vector<std::shared_ptr<const rvsdg::type>> types)
      : Types_(std::move(types))
  {}

  Declaration() = default;

  Declaration(const Declaration &) = default;

  Declaration &
  operator=(const Declaration &) = default;

  [[nodiscard]] size_t
  NumElements() const noexcept
  {
    return Types_.size();
  }

  [[nodiscard]] const valuetype &
  GetElement(size_t index) const noexcept
  {
    JLM_ASSERT(index < NumElements());
    return *util::AssertedCast<const valuetype>(Types_[index].get());
  }

  void
  Append(const jlm::rvsdg::valuetype & type)
  {
    Types_.push_back(type.copy());
  }

  static std::unique_ptr<Declaration>
  Create()
  {
    return std::unique_ptr<Declaration>(new Declaration());
  }

  static std::unique_ptr<Declaration>
  Create(std::vector<std::shared_ptr<const rvsdg::type>> types)
  {
    return std::make_unique<Declaration>(std::move(types));
  }

private:
  std::vector<std::shared_ptr<const rvsdg::type>> Types_;
};

/* vector type */

class vectortype : public jlm::rvsdg::valuetype
{
public:
  vectortype(std::shared_ptr<const jlm::rvsdg::valuetype> type, size_t size)
      : size_(size),
        type_(std::move(type))
  {}

  vectortype(const vectortype & other) = default;

  vectortype(vectortype && other) = default;

  vectortype &
  operator=(const vectortype & other) = default;

  vectortype &
  operator=(vectortype && other) = default;

  virtual bool
  operator==(const jlm::rvsdg::type & other) const noexcept override;

  size_t
  size() const noexcept
  {
    return size_;
  }

  const jlm::rvsdg::valuetype &
  type() const noexcept
  {
    return *type_;
  }

  const std::shared_ptr<const rvsdg::valuetype> &
  Type() const noexcept
  {
    return type_;
  }

private:
  size_t size_;
  std::shared_ptr<const jlm::rvsdg::valuetype> type_;
};

class fixedvectortype final : public vectortype
{
public:
  ~fixedvectortype() override;

  fixedvectortype(std::shared_ptr<const jlm::rvsdg::valuetype> type, size_t size)
      : vectortype(std::move(type), size)
  {}

  virtual bool
  operator==(const jlm::rvsdg::type & other) const noexcept override;

  std::shared_ptr<const jlm::rvsdg::type>
  copy() const override;

  virtual std::string
  debug_string() const override;

  static std::shared_ptr<const fixedvectortype>
  Create(std::shared_ptr<const jlm::rvsdg::valuetype> type, size_t size)
  {
    return std::make_shared<fixedvectortype>(std::move(type), size);
  }
};

class scalablevectortype final : public vectortype
{
public:
  ~scalablevectortype() override;

  scalablevectortype(std::shared_ptr<const jlm::rvsdg::valuetype> type, size_t size)
      : vectortype(std::move(type), size)
  {}

  virtual bool
  operator==(const jlm::rvsdg::type & other) const noexcept override;

  std::shared_ptr<const jlm::rvsdg::type>
  copy() const override;

  virtual std::string
  debug_string() const override;

  static std::shared_ptr<const scalablevectortype>
  Create(std::shared_ptr<const jlm::rvsdg::valuetype> type, size_t size)
  {
    return std::make_shared<scalablevectortype>(std::move(type), size);
  }
};

/** \brief Input/Output state type
 *
 * This type is used for state edges that sequentialize input/output operations.
 */
class iostatetype final : public jlm::rvsdg::statetype
{
public:
  ~iostatetype() override;

  constexpr iostatetype() noexcept
  {}

  virtual bool
  operator==(const jlm::rvsdg::type & other) const noexcept override;

  std::shared_ptr<const jlm::rvsdg::type>
  copy() const override;

  virtual std::string
  debug_string() const override;

  static std::shared_ptr<const iostatetype>
  Create();
};

/** \brief Memory state type class
 *
 * Represents the type of abstract memory locations and is used in state edges for sequentialiazing
 * memory operations, such as load and store operations.
 */
class MemoryStateType final : public jlm::rvsdg::statetype
{
public:
  ~MemoryStateType() noexcept override;

  constexpr MemoryStateType() noexcept
      : jlm::rvsdg::statetype()
  {}

  std::string
  debug_string() const override;

  bool
  operator==(const jlm::rvsdg::type & other) const noexcept override;

  std::shared_ptr<const jlm::rvsdg::type>
  copy() const override;

  static std::shared_ptr<const MemoryStateType>
  Create();
};

template<class ELEMENTYPE>
inline bool
IsOrContains(const jlm::rvsdg::type & type)
{
  if (jlm::rvsdg::is<ELEMENTYPE>(type))
    return true;

  if (auto arrayType = dynamic_cast<const arraytype *>(&type))
    return IsOrContains<ELEMENTYPE>(arrayType->element_type());

  if (auto structType = dynamic_cast<const StructType *>(&type))
  {
    auto & structDeclaration = structType->GetDeclaration();
    for (size_t n = 0; n < structDeclaration.NumElements(); n++)
      if (IsOrContains<ELEMENTYPE>(structDeclaration.GetElement(n)))
        return true;

    return false;
  }

  if (auto vectorType = dynamic_cast<const vectortype *>(&type))
    return IsOrContains<ELEMENTYPE>(vectorType->type());

  return false;
}

/**
 * Given a type, determines if it is one of LLVM's aggregate types.
 * Vectors are not considered to be aggregate types, despite being based on a subtype.
 * @param type the type to check
 * @return true if the type is an aggreate type, false otherwise
 */
inline bool
IsAggregateType(const jlm::rvsdg::type & type)
{
  return jlm::rvsdg::is<arraytype>(type) || jlm::rvsdg::is<StructType>(type);
}

}

#endif

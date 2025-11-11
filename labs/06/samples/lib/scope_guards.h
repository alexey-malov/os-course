#pragma once
#include <exception> // std::uncaught_exceptions
#include <type_traits>
#include <utility>

namespace sg
{

namespace detail
{

template <class Fn>
concept GuardFn = std::is_nothrow_invocable_v<Fn> && std::is_nothrow_move_constructible_v<Fn>;

template <GuardFn Fn>
class ScopeGuardBase
{
public:
	explicit ScopeGuardBase(Fn&& fn) noexcept
		: m_fn(std::move(fn))
	{
	}

	ScopeGuardBase(const ScopeGuardBase&) = delete;
	ScopeGuardBase& operator=(const ScopeGuardBase&) = delete;

	ScopeGuardBase(ScopeGuardBase&& other) noexcept
		: m_fn(std::move(other.m_fn))
		, m_active(std::exchange(other.m_active, false))
	{
	}

	ScopeGuardBase& operator=(ScopeGuardBase&&) = delete;

	void Release() noexcept { m_active = false; }
	[[nodiscard]] bool Active() const noexcept { return m_active; }

protected:
	void ExecFn() noexcept
	{
		m_fn();
	}

private:
	[[no_unique_address]] Fn m_fn;
	bool m_active{ true };
};

template <GuardFn Fn>
class ScopeTryBase : public ScopeGuardBase<Fn>
{
	using Base = ScopeGuardBase<Fn>;

public:
	using Base::Base;

protected:
	int m_enter = std::uncaught_exceptions();
};

} // namespace detail

/**
 * @brief Runs a callback on every scope exit.
 *
 * Executes the stored functor unconditionally when the guard is destroyed.
 * The functor must be nothrow-invocable and nothrow-move-constructible.
 *
 * Move-only: copy operations are deleted; move construction transfers ownership.
 * Call Release() to disable execution. Active() reports state.
 *
 * @tparam Fn Nothrow-invocable, nothrow-move-constructible callable type.
 */
template <detail::GuardFn Fn>
class ScopeExit final : public detail::ScopeGuardBase<Fn>
{
	using Base = detail::ScopeGuardBase<Fn>;

public:
	using Base::Base;

	// Явно просим компилятор сгенерировать перемещающий конструктор,
	// так как при наличии деструктора он не будет этого делать.
	ScopeExit(ScopeExit&& other) noexcept = default;

	~ScopeExit()
	{
		if (this->Active())
		{
			this->ExecFn();
		}
	}
};

/**
 * @brief Runs a callback only on successful scope exit (no exception in flight).
 *
 * Captures std::uncaught_exceptions() on construction and executes the functor
 * at destruction only if the counter has not increased (i.e., no exception is
 * propagating). The functor must be nothrow-invocable and nothrow-move-constructible.
 *
 * Move-only: copy operations are deleted; move construction transfers ownership.
 * Call Release() to disable execution. Active() reports state.
 *
 * @tparam Fn Nothrow-invocable, nothrow-move-constructible callable type.
 */
template <detail::GuardFn Fn>
class ScopeSuccess final : public detail::ScopeTryBase<Fn>
{
	using Base = detail::ScopeTryBase<Fn>;

public:
	using Base::Base;

	ScopeSuccess(ScopeSuccess&& other) noexcept = default;

	~ScopeSuccess()
	{
		if (this->Active() && std::uncaught_exceptions() == this->m_enter)
		{
			this->ExecFn();
		}
	}
};

/**
 * @brief Runs a callback only on failure (scope exits due to an exception).
 *
 * Captures std::uncaught_exceptions() on construction and executes the functor
 * at destruction only if the counter has increased (i.e., an exception is
 * propagating). The functor must be nothrow-invocable and nothrow-move-constructible.
 *
 * Move-only: copy operations are deleted; move construction transfers ownership.
 * Call Release() to disable execution. Active() reports state.
 *
 * @tparam Fn Nothrow-invocable, nothrow-move-constructible callable type.
 */
template <detail::GuardFn Fn>
class ScopeFail final : public detail::ScopeTryBase<Fn>
{
	using Base = detail::ScopeTryBase<Fn>;

public:
	using Base::Base;

	ScopeFail(ScopeFail&& other) noexcept = default;

	~ScopeFail()
	{
		if (this->Active() && std::uncaught_exceptions() > this->m_enter)
		{
			this->ExecFn();
		}
	}
};

// Guides: учим компилятор, как выводить Fn при Class Template Argument Deduction

template <class F>
ScopeExit(F) -> ScopeExit<F>;

template <class F>
ScopeSuccess(F) -> ScopeSuccess<F>;

template <class F>
ScopeFail(F) -> ScopeFail<F>;

/*** @brief Factory for a scope guard that runs on every scope exit.
 *
 * Constructs a ScopeExit from the given callable, forwarding and decaying it.
 * No dynamic allocation. The callable must be nothrow-invocable and
 * nothrow-move-constructible.
 *
 * @tparam F   Callable type (decayed to the guard's Fn).
 * @param  f   Callback to execute at scope exit.
 * @return ScopeExit<std::decay_t<F>>  Move-only guard. Call Release() to disable.
 *
 * @see ScopeExit
 */
template <class F>
	requires detail::GuardFn<std::decay_t<F>>
[[nodiscard]] constexpr auto MakeScopeExit(F&& f) noexcept -> ScopeExit<std::decay_t<F>>
{
	return ScopeExit<std::decay_t<F>>{ std::forward<F>(f) };
}

/**
 * @brief Factory for a scope guard that runs only on successful scope exit.
 *
 * Constructs a ScopeSuccess which invokes the callback at destruction if
 * no exception is in flight (based on std::uncaught_exceptions()).
 * The callable must be nothrow-invocable and nothrow-move-constructible.
 *
 * @tparam F   Callable type (decayed to the guard's Fn).
 * @param  f   Callback to execute on success.
 * @return ScopeSuccess<std::decay_t<F>>  Move-only guard. Call Release() to disable.
 *
 * @see ScopeSuccess
 */
template <class F>
	requires detail::GuardFn<std::decay_t<F>>
[[nodiscard]] constexpr auto MakeScopeSuccess(F&& f) noexcept -> ScopeSuccess<std::decay_t<F>>
{
	return ScopeSuccess<std::decay_t<F>>{ std::forward<F>(f) };
}

/**
 * @brief Factory for a scope guard that runs only on failure (exceptional exit).
 *
 * Constructs a ScopeFail which invokes the callback at destruction if
 * an exception is propagating (based on std::uncaught_exceptions()).
 * The callable must be nothrow-invocable and nothrow-move-constructible.
 *
 * @tparam F   Callable type (decayed to the guard's Fn).
 * @param  f   Callback to execute on failure.
 * @return ScopeFail<std::decay_t<F>>  Move-only guard. Call Release() to disable.
 *
 * @see ScopeFail
 */
template <class F>
	requires detail::GuardFn<std::decay_t<F>>
[[nodiscard]] constexpr auto MakeScopeFail(F&& f) noexcept -> ScopeFail<std::decay_t<F>>
{
	return ScopeFail<std::decay_t<F>>{ std::forward<F>(f) };
}

} // namespace sg

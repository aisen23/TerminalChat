#include <utility>

namespace tc::utils
{
	template<typename F>
	class ScopeGuard {
	public:
		explicit ScopeGuard(F&& f)
			: func(std::forward<F>(f)), active(true)
		{}

		~ScopeGuard()
		{
			if (active)
				func();
		}

		ScopeGuard(const ScopeGuard&) = delete;
		ScopeGuard& operator=(const ScopeGuard&) = delete;

		ScopeGuard(ScopeGuard&& other) noexcept
			: func(std::move(other.func)), active(other.active)
		{
			other.dismiss();
		}

		void dismiss() noexcept
		{
			active = false;
		}

	private:
		F func;
		bool active;
	};

	template<typename F>
	ScopeGuard<F> makeScopeGuard(F&& f)
	{
		return ScopeGuard<F>(std::forward<F>(f));
	}
}

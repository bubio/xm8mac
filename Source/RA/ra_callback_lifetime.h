#ifndef XM8_RA_CALLBACK_LIFETIME_H
#define XM8_RA_CALLBACK_LIFETIME_H

#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <utility>

namespace Xm8Ra {

// Keeps an owner alive logically while callbacks are executing. The owner
// still controls its storage; CloseAndWait prevents new leases and waits until
// every already-issued lease has been released.
class RaCallbackLifetime {
private:
	struct State {
		explicit State(void *value) : owner(value) {}
		std::mutex mutex;
		std::condition_variable idle;
		void *owner;
		size_t callbacks = 0;
		bool closed = false;
	};

public:
	class Lease {
	public:
		Lease() : owner_(nullptr) {}
		Lease(Lease&& source) noexcept :
			state_(std::move(source.state_)), owner_(source.owner_)
		{
			source.owner_ = nullptr;
		}
		Lease& operator=(Lease&& source) noexcept
		{
			if (this != &source) {
				Release();
				state_ = std::move(source.state_);
				owner_ = source.owner_;
				source.owner_ = nullptr;
			}
			return *this;
		}
		~Lease() { Release(); }

		Lease(const Lease&) = delete;
		Lease& operator=(const Lease&) = delete;

		explicit operator bool() const { return owner_ != nullptr; }
		template<typename T> T *Owner() const
		{
			return static_cast<T *>(owner_);
		}

	private:
		friend class RaCallbackLifetime;
		Lease(const std::shared_ptr<State>& state, void *owner) :
			state_(state), owner_(owner) {}
		void Release()
		{
			if (!state_) return;
			std::shared_ptr<State> state = std::move(state_);
			{
				std::lock_guard<std::mutex> lock(state->mutex);
				if (--state->callbacks == 0) state->idle.notify_all();
			}
			owner_ = nullptr;
		}

		std::shared_ptr<State> state_;
		void *owner_;
	};

	explicit RaCallbackLifetime(void *owner) : state_(new State(owner)) {}

	Lease TryAcquire() const
	{
		std::lock_guard<std::mutex> lock(state_->mutex);
		if (state_->closed || state_->owner == nullptr) return Lease();
		++state_->callbacks;
		return Lease(state_, state_->owner);
	}

	void CloseAndWait()
	{
		std::unique_lock<std::mutex> lock(state_->mutex);
		state_->closed = true;
		state_->owner = nullptr;
		state_->idle.wait(lock, [this]() { return state_->callbacks == 0; });
	}

private:
	std::shared_ptr<State> state_;
};

} // namespace Xm8Ra

#endif

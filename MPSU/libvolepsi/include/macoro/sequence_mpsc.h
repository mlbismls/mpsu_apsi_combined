///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include "macoro/config.h"
#include "macoro/sequence_barrier.h"
#include "macoro/sequence_range.h"
#include "macoro/sequence_traits.h"

#include "macoro/detail/manual_lifetime.h"

#include <atomic>
#include <cstdint>
#include <cassert>

namespace macoro
{
	template<typename SEQUENCE, typename TRAITS>
	class sequence_mpsc_claim_one_operation;

	template<typename SEQUENCE, typename TRAITS>
	class sequence_mpsc_claim_operation;

	template<typename SEQUENCE, typename TRAITS>
	class sequence_mpsc_wait_operation_base;

	//template<typename SEQUENCE, typename TRAITS>
	//class sequence_mpsc_wait_operation;

	/// A multi-producer sequencer is a thread-synchronization primitive that can be
	/// used to synchronize access to a ring-buffer of power-of-two size where you
	/// have multiple producers concurrently claiming slots in the ring-buffer and
	/// publishing items.
	///
	/// When a writer wants to write to a slot in the buffer it first atomically
	/// increments a counter by the number of slots it wishes to allocate.
	/// It then waits until all of those slots have become available and then
	/// returns the range of sequence numbers allocated back to the caller.
	/// The caller then writes to those slots and when done publishes them by
	/// writing the sequence numbers published to each of the slots to the
	/// corresponding element of an array of equal size to the ring buffer.
	/// When a reader wants to check if the next sequence number is available
	/// it then simply needs to read from the corresponding slot in this array
	/// to check if the value stored there is equal to the sequence number it
	/// is wanting to read.
	///
	/// This means concurrent writers are wait-free when there is space available
	/// in the ring buffer, requiring a single atomic fetch-add operation as the
	/// only contended write operation. All other writes are to memory locations
	/// owned by a particular writer. Concurrent writers can publish items out of
	/// order so that one writer does not hold up other writers until the ring
	/// buffer fills up.
	template<
		typename SEQUENCE = std::size_t,
		typename TRAITS = sequence_traits<SEQUENCE>>
	class sequence_mpsc
	{
	public:
		static constexpr bool multi_sender = true;

		sequence_mpsc(
			const sequence_barrier<SEQUENCE, TRAITS>& consumerBarrier,
			std::size_t bufferSize,
			SEQUENCE initialSequence = TRAITS::initial_sequence);

		/// The size of the circular buffer. This will be a power-of-two.
		std::size_t buffer_size() const noexcept { return m_sequenceMask + 1; }

		/// Lookup the last-known-published sequence number after the specified
		/// sequence number.
		SEQUENCE last_published_after(SEQUENCE lastKnownPublished) const noexcept;

		/// Wait until the specified target sequence number has been published.
		///
		/// Returns an awaitable type that when co_awaited will suspend the awaiting
		/// coroutine until the specified 'targetSequence' number and all prior sequence
		/// numbers have been published.
		sequence_mpsc_wait_operation_base<SEQUENCE, TRAITS> wait_until_published(
			SEQUENCE targetSequence,
			SEQUENCE lastKnownPublished) const noexcept;

		/// Query if there are currently any slots available for claiming.
		///
		/// Note that this return-value is only approximate if you have multiple producers
		/// since immediately after returning true another thread may have claimed the
		/// last available slot.
		bool any_available() const noexcept;

		/// Claim a single slot in the buffer and wait until that slot becomes available.
		///
		/// Returns an Awaitable type that yields the sequence number of the slot that
		/// was claimed.
		///
		/// Once the producer has claimed a slot then they are free to write to that
		/// slot within the ring buffer. Once the value has been initialised the item
		/// must be published by calling the .publish() method, passing the sequence
		/// number.
		sequence_mpsc_claim_one_operation<SEQUENCE, TRAITS>
		claim_one() noexcept;

		/// Claim a contiguous range of sequence numbers corresponding to slots within
		/// a ring-buffer.
		///
		/// This will claim at most the specified count of sequence numbers but may claim
		/// fewer if there are only fewer entries available in the buffer. But will claim
		/// at least one sequence number.
		///
		/// Returns an awaitable that will yield a sequence_range object containing the
		/// sequence numbers that were claimed.
		///
		/// The caller is responsible for ensuring that they publish every element of the
		/// returned sequence range by calling .publish().
		sequence_mpsc_claim_operation<SEQUENCE, TRAITS>
		claim_up_to(std::size_t count) noexcept;

		/// Publish the element with the specified sequence number, making it available
		/// to consumers.
		///
		/// Note that different sequence numbers may be published by different producer
		/// threads out of order. A sequence number will not become available to consumers
		/// until all preceding sequence numbers have also been published.
		///
		/// \param sequence
		/// The sequence number of the element to publish
		/// This sequence number must have been previously acquired via a call to 'claim_one()'
		/// or 'claim_up_to()'.
		void publish(SEQUENCE sequence) noexcept;

		/// Publish a contiguous range of sequence numbers, making each of them available
		/// to consumers.
		///
		/// This is equivalent to calling publish(seq) for each sequence number, seq, in
		/// the specified range, but is more efficient since it only checks to see if
		/// there are coroutines that need to be woken up once.
		void publish(const sequence_range<SEQUENCE, TRAITS>& range) noexcept;

	private:

		template<typename SEQUENCE2, typename TRAITS2>
		friend class sequence_mpsc_wait_operation_base;

		template<typename SEQUENCE2, typename TRAITS2>
		friend class sequence_mpsc_claim_operation;

		template<typename SEQUENCE2, typename TRAITS2>
		friend class sequence_mpsc_claim_one_operation;

		void resume_ready_awaiters() noexcept;
		void add_awaiter(sequence_mpsc_wait_operation_base<SEQUENCE, TRAITS>* awaiter) const noexcept;

#if _MSC_VER
# pragma warning(push)
# pragma warning(disable : 4324) // C4324: structure was padded due to alignment specifier
#endif

		const sequence_barrier<SEQUENCE, TRAITS>& m_consumerBarrier;
		const std::size_t m_sequenceMask;
		const std::unique_ptr<std::atomic<SEQUENCE>[]> m_published;

		alignas(MACORO_CPU_CACHE_LINE)
		std::atomic<SEQUENCE> m_nextToClaim;

		alignas(MACORO_CPU_CACHE_LINE)
		mutable std::atomic<sequence_mpsc_wait_operation_base<SEQUENCE, TRAITS>*> m_awaiters;

#if _MSC_VER
# pragma warning(pop)
#endif

	};

	template<typename SEQUENCE, typename TRAITS>
	class sequence_mpsc_claim_awaiter
	{
	public:

		sequence_mpsc_claim_awaiter(
			const sequence_barrier<SEQUENCE, TRAITS>& consumerBarrier,
			std::size_t bufferSize,
			const sequence_range<SEQUENCE, TRAITS>& claimedRange) noexcept
			: m_barrierWait(consumerBarrier, claimedRange.back() - bufferSize)
			, m_claimedRange(claimedRange)
		{}

		bool await_ready() const noexcept
		{
			return m_barrierWait.await_ready();
		}

#ifdef MACORO_CPP_20
		auto await_suspend(std::coroutine_handle<> awaitingCoroutine) noexcept
		{
			return await_suspend(coroutine_handle<>(awaitingCoroutine));
		}
#endif
		auto await_suspend(coroutine_handle<> awaitingCoroutine) noexcept
		{
			return m_barrierWait.await_suspend(awaitingCoroutine);
		}

		sequence_range<SEQUENCE, TRAITS> await_resume() noexcept
		{
			return m_claimedRange;
		}

	private:

		sequence_barrier_wait_operation_base<SEQUENCE, TRAITS> m_barrierWait;
		sequence_range<SEQUENCE, TRAITS> m_claimedRange;

	};

	template<typename SEQUENCE, typename TRAITS>
	class sequence_mpsc_claim_operation
	{
	public:

		sequence_mpsc_claim_operation(
			sequence_mpsc<SEQUENCE, TRAITS>& sequencer,
			std::size_t count) noexcept
			: m_sequencer(sequencer)
			, m_count(count < sequencer.buffer_size() ? count : sequencer.buffer_size())
		{
		}

		sequence_mpsc_claim_awaiter<SEQUENCE, TRAITS> MACORO_OPERATOR_COAWAIT() noexcept
		{
			// We wait until the awaitable is actually co_await'ed before we claim the
			// range of elements. If we claimed them earlier, then it may be possible for
			// the caller to fail to co_await the result eg. due to an exception, which
			// would leave the sequence numbers unable to be published and would eventually
			// deadlock consumers that waited on them.
			//
			// TODO: We could try and acquire only as many as are available if fewer than
			// m_count elements are available. This would complicate the logic here somewhat
			// as we'd need to use a compare-exchange instead.
			const SEQUENCE first = m_sequencer.m_nextToClaim.fetch_add(m_count, std::memory_order_relaxed);
			return sequence_mpsc_claim_awaiter<SEQUENCE, TRAITS>{
				m_sequencer.m_consumerBarrier,
				m_sequencer.buffer_size(),
				sequence_range<SEQUENCE, TRAITS>{ first, first + m_count }
			};
		}

	private:

		sequence_mpsc<SEQUENCE, TRAITS>& m_sequencer;
		std::size_t m_count;

	};

	template<typename SEQUENCE, typename TRAITS>
	class sequence_mpsc_claim_one_awaiter
	{
	public:

		sequence_mpsc_claim_one_awaiter(
			const sequence_barrier<SEQUENCE, TRAITS>& consumerBarrier,
			std::size_t bufferSize,
			SEQUENCE claimedSequence) noexcept
			: m_waitOp(consumerBarrier, claimedSequence - bufferSize)
			, m_claimedSequence(claimedSequence)
		{}

		bool await_ready() const noexcept
		{
			return m_waitOp.await_ready();
		}

#ifdef MACORO_CPP_20
		auto await_suspend(std::coroutine_handle<> awaitingCoroutine) noexcept
		{
			return await_suspend(coroutine_handle<>(awaitingCoroutine));
		}
#endif
		auto await_suspend(coroutine_handle<> awaitingCoroutine) noexcept
		{
			return m_waitOp.await_suspend(awaitingCoroutine);
		}

		SEQUENCE await_resume() noexcept
		{
			return m_claimedSequence;
		}

	private:

		sequence_barrier_wait_operation_base<SEQUENCE, TRAITS> m_waitOp;
		SEQUENCE m_claimedSequence;

	};

	template<typename SEQUENCE, typename TRAITS>
	class sequence_mpsc_claim_one_operation
	{
	public:
		using Awaiter = sequence_mpsc_claim_one_awaiter<SEQUENCE, TRAITS>;

		sequence_mpsc_claim_one_operation(
			sequence_mpsc<SEQUENCE, TRAITS>& sequencer) noexcept
			: m_sequencer(sequencer)
		{}

		sequence_mpsc_claim_one_awaiter<SEQUENCE, TRAITS> MACORO_OPERATOR_COAWAIT() noexcept
		{
			return sequence_mpsc_claim_one_awaiter<SEQUENCE, TRAITS>{
				m_sequencer.m_consumerBarrier,
				m_sequencer.buffer_size(),
				m_sequencer.m_nextToClaim.fetch_add(1, std::memory_order_relaxed)
			};
		}

	private:

		sequence_mpsc<SEQUENCE, TRAITS>& m_sequencer;

	};

	template<typename SEQUENCE, typename TRAITS>
	class sequence_mpsc_wait_operation_base
	{
	public:

		sequence_mpsc_wait_operation_base(
			const sequence_mpsc<SEQUENCE, TRAITS>& sequencer,
			SEQUENCE targetSequence,
			SEQUENCE lastKnownPublished) noexcept
			: m_sequencer(sequencer)
			, m_targetSequence(targetSequence)
			, m_lastKnownPublished(lastKnownPublished)
			, m_readyToResume(false)
		{}

		sequence_mpsc_wait_operation_base(
			const sequence_mpsc_wait_operation_base& other) noexcept
			: m_sequencer(other.m_sequencer)
			, m_targetSequence(other.m_targetSequence)
			, m_lastKnownPublished(other.m_lastKnownPublished)
			, m_readyToResume(false)
		{}

		bool await_ready() const noexcept
		{
			return !TRAITS::precedes(m_lastKnownPublished, m_targetSequence);
		}

#ifdef MACORO_CPP_20
		auto await_suspend(std::coroutine_handle<> awaitingCoroutine) noexcept
		{
			return await_suspend(coroutine_handle<>(awaitingCoroutine));
		}
#endif
		bool await_suspend(coroutine_handle<> awaitingCoroutine) noexcept
		{
			m_awaitingCoroutine = awaitingCoroutine;

			m_sequencer.add_awaiter(this);

			// Mark the waiter as ready to resume.
			// If it was already marked as ready-to-resume within the call to add_awaiter() or
			// on another thread then this exchange() will return true. In this case we want to
			// resume immediately and continue execution by returning false.
			return !m_readyToResume.exchange(true, std::memory_order_acquire);
		}

		SEQUENCE await_resume() noexcept
		{
			return m_lastKnownPublished;
		}

	protected:

		friend class sequence_mpsc<SEQUENCE, TRAITS>;

		void resume(SEQUENCE lastKnownPublished) noexcept
		{
			m_lastKnownPublished = lastKnownPublished;
			if (m_readyToResume.exchange(true, std::memory_order_release))
			{
				resume_impl();
			}
		}

		void resume_impl() noexcept
		{
			this->m_awaitingCoroutine.resume();
		};

		const sequence_mpsc<SEQUENCE, TRAITS>& m_sequencer;
		SEQUENCE m_targetSequence;
		SEQUENCE m_lastKnownPublished;
		sequence_mpsc_wait_operation_base* m_next;
		coroutine_handle<> m_awaitingCoroutine;
		std::atomic<bool> m_readyToResume;
	};

	template<typename SEQUENCE, typename TRAITS>
	sequence_mpsc<SEQUENCE, TRAITS>::sequence_mpsc(
		const sequence_barrier<SEQUENCE, TRAITS>& consumerBarrier,
		std::size_t bufferSize,
		SEQUENCE initialSequence)
		: m_consumerBarrier(consumerBarrier)
		, m_sequenceMask(bufferSize - 1)
		, m_published(std::make_unique<std::atomic<SEQUENCE>[]>(bufferSize))
		, m_nextToClaim(initialSequence + 1)
		, m_awaiters(nullptr)
	{
		// bufferSize must be a positive power-of-two
		assert(bufferSize > 0 && (bufferSize & (bufferSize - 1)) == 0);
		// but must be no larger than the max diff value.
		using diff_t = typename TRAITS::difference_type;
		using unsigned_diff_t = std::make_unsigned_t<diff_t>;
		constexpr unsigned_diff_t maxSize = static_cast<unsigned_diff_t>(std::numeric_limits<diff_t>::max());
		assert(bufferSize <= maxSize);

		SEQUENCE seq = initialSequence - (bufferSize - 1);
		do
		{
#ifdef __cpp_lib_atomic_value_initialization
			m_published[seq & m_sequenceMask].store(seq, std::memory_order_relaxed);
#else // ^^^ __cpp_lib_atomic_value_initialization // !__cpp_lib_atomic_value_initialization vvv
			std::atomic_init(&m_published[seq & m_sequenceMask], seq);
#endif // !__cpp_lib_atomic_value_initialization
		} while (seq++ != initialSequence);
	}

	template<typename SEQUENCE, typename TRAITS>
	SEQUENCE sequence_mpsc<SEQUENCE, TRAITS>::last_published_after(
		SEQUENCE lastKnownPublished) const noexcept
	{
		const auto mask = m_sequenceMask;
		SEQUENCE seq = lastKnownPublished + 1;
		while (m_published[seq & mask].load(std::memory_order_acquire) == seq)
		{
			lastKnownPublished = seq++;
		}
		return lastKnownPublished;
	}

	template<typename SEQUENCE, typename TRAITS>
	sequence_mpsc_wait_operation_base<SEQUENCE, TRAITS>
	sequence_mpsc<SEQUENCE, TRAITS>::wait_until_published(
		SEQUENCE targetSequence,
		SEQUENCE lastKnownPublished) const noexcept
	{
		return sequence_mpsc_wait_operation_base<SEQUENCE, TRAITS>{
			*this, targetSequence, lastKnownPublished
		};
	}

	template<typename SEQUENCE, typename TRAITS>
	bool sequence_mpsc<SEQUENCE, TRAITS>::any_available() const noexcept
	{
		return TRAITS::precedes(
			m_nextToClaim.load(std::memory_order_relaxed),
			m_consumerBarrier.last_published() + buffer_size());
	}

	template<typename SEQUENCE, typename TRAITS>
	sequence_mpsc_claim_one_operation<SEQUENCE, TRAITS>
	sequence_mpsc<SEQUENCE, TRAITS>::claim_one() noexcept
	{
		return sequence_mpsc_claim_one_operation<SEQUENCE, TRAITS>{ *this };
	}

	template<typename SEQUENCE, typename TRAITS>
	sequence_mpsc_claim_operation<SEQUENCE, TRAITS>
	sequence_mpsc<SEQUENCE, TRAITS>::claim_up_to(std::size_t count) noexcept
	{
		return sequence_mpsc_claim_operation<SEQUENCE, TRAITS>{ *this, count };
	}

	template<typename SEQUENCE, typename TRAITS>
	void sequence_mpsc<SEQUENCE, TRAITS>::publish(SEQUENCE sequence) noexcept
	{
		m_published[sequence & m_sequenceMask].store(sequence, std::memory_order_seq_cst);

		// Resume any waiters that might have been satisfied by this publish operation.
		resume_ready_awaiters();
	}

	template<typename SEQUENCE, typename TRAITS>
	void sequence_mpsc<SEQUENCE, TRAITS>::publish(const sequence_range<SEQUENCE, TRAITS>& range) noexcept
	{
		if (range.empty())
		{
			return;
		}

		// Publish all but the first sequence number using relaxed atomics.
		// No consumer should be reading those subsequent sequence numbers until they've seen
		// that the first sequence number in the range is published.
		for (SEQUENCE seq : range.skip(1))
		{
			m_published[seq & m_sequenceMask].store(seq, std::memory_order_relaxed);
		}

		// Now publish the first sequence number with seq_cst semantics.
		m_published[range.front() & m_sequenceMask].store(range.front(), std::memory_order_seq_cst);

		// Resume any waiters that might have been satisfied by this publish operation.
		resume_ready_awaiters();
	}

	template<typename SEQUENCE, typename TRAITS>
	void sequence_mpsc<SEQUENCE, TRAITS>::resume_ready_awaiters() noexcept
	{
		using awaiter_t = sequence_mpsc_wait_operation_base<SEQUENCE, TRAITS>;

		awaiter_t* awaiters = m_awaiters.load(std::memory_order_seq_cst);
		if (awaiters == nullptr)
		{
			// No awaiters
			return;
		}

		// There were some awaiters. Try to acquire the list of waiters with an
		// atomic exchange as we might be racing with other consumers/producers.
		awaiters = m_awaiters.exchange(nullptr, std::memory_order_seq_cst);
		if (awaiters == nullptr)
		{
			// Didn't acquire the list
			// Some other thread is now responsible for resuming them. Our job is done.
			return;
		}

		SEQUENCE lastKnownPublished;

		awaiter_t* awaitersToResume;
		awaiter_t** awaitersToResumeTail = &awaitersToResume;

		awaiter_t* awaitersToRequeue;
		awaiter_t** awaitersToRequeueTail = &awaitersToRequeue;

		do
		{
			using diff_t = typename TRAITS::difference_type;

			lastKnownPublished = last_published_after(awaiters->m_lastKnownPublished);

			// First scan the list of awaiters and split them into 'requeue' and 'resume' lists.
			auto minDiff = std::numeric_limits<diff_t>::max();
			do
			{
				auto diff = TRAITS::difference(awaiters->m_targetSequence, lastKnownPublished);
				if (diff > 0)
				{
					// Not ready yet.
					minDiff = diff < minDiff ? diff : minDiff;
					*awaitersToRequeueTail = awaiters;
					awaitersToRequeueTail = &awaiters->m_next;
				}
				else
				{
					*awaitersToResumeTail = awaiters;
					awaitersToResumeTail = &awaiters->m_next;
				}
				awaiters->m_lastKnownPublished = lastKnownPublished;
				awaiters = awaiters->m_next;
			} while (awaiters != nullptr);

			// Null-terinate the requeue list
			*awaitersToRequeueTail = nullptr;

			if (awaitersToRequeue != nullptr)
			{
				// Requeue the waiters that are not ready yet.
				awaiter_t* oldHead = nullptr;
				while (!m_awaiters.compare_exchange_weak(oldHead, awaitersToRequeue, std::memory_order_seq_cst, std::memory_order_relaxed))
				{
					*awaitersToRequeueTail = oldHead;
				}

				// Reset the awaitersToRequeue list
				awaitersToRequeueTail = &awaitersToRequeue;

				const SEQUENCE earliestTargetSequence = lastKnownPublished + minDiff;

				// Now we need to check again to see if any of the waiters we just enqueued
				// is now satisfied by a concurrent call to publish().
				//
				// We need to be a bit more careful here since we are no longer holding any
				// awaiters and so producers/consumers may advance the sequence number arbitrarily
				// far. If the sequence number advances more than buffer_size() ahead of the
				// earliestTargetSequence then the m_published[] array may have sequence numbers
				// that have advanced beyond earliestTargetSequence, potentially even wrapping
				// sequence numbers around to then be preceding where they were before. If this
				// happens then we don't need to worry about resuming any awaiters that were waiting
				// for 'earliestTargetSequence' since some other thread has already resumed them.
				// So the only case we need to worry about here is when all m_published entries for
				// sequence numbers in range [lastKnownPublished + 1, earliestTargetSequence] have
				// published sequence numbers that match the range.
				const auto sequenceMask = m_sequenceMask;
				SEQUENCE seq = lastKnownPublished + 1;
				while (m_published[seq & sequenceMask].load(std::memory_order_seq_cst) == seq)
				{
					lastKnownPublished = seq;
					if (seq == earliestTargetSequence)
					{
						// At least one of the awaiters we just published is now satisfied.
						// Reacquire the list of awaiters and continue around the outer loop.
						awaiters = m_awaiters.exchange(nullptr, std::memory_order_acquire);
						break;
					}
					++seq;
				}
			}
		} while (awaiters != nullptr);

		// Null-terminate list of awaiters to resume.
		*awaitersToResumeTail = nullptr;

		while (awaitersToResume != nullptr)
		{
			awaiter_t* next = awaitersToResume->m_next;
			awaitersToResume->resume(lastKnownPublished);
			awaitersToResume = next;
		}
	}

	template<typename SEQUENCE, typename TRAITS>
	void sequence_mpsc<SEQUENCE, TRAITS>::add_awaiter(
		sequence_mpsc_wait_operation_base<SEQUENCE, TRAITS>* awaiter) const noexcept
	{
		using awaiter_t = sequence_mpsc_wait_operation_base<SEQUENCE, TRAITS>;

		SEQUENCE targetSequence = awaiter->m_targetSequence;
		SEQUENCE lastKnownPublished = awaiter->m_lastKnownPublished;

		awaiter_t* awaitersToEnqueue = awaiter;
		awaiter_t** awaitersToEnqueueTail = &awaiter->m_next;

		awaiter_t* awaitersToResume;
		awaiter_t** awaitersToResumeTail = &awaitersToResume;

		const SEQUENCE sequenceMask = m_sequenceMask;

		do
		{
			// Enqueue the awaiters.
			{
				awaiter_t* oldHead = m_awaiters.load(std::memory_order_relaxed);
				do
				{
					*awaitersToEnqueueTail = oldHead;
				} while (!m_awaiters.compare_exchange_weak(
					oldHead,
					awaitersToEnqueue,
					std::memory_order_seq_cst,
					std::memory_order_relaxed));
			}

			// Reset list of waiters
			awaitersToEnqueueTail = &awaitersToEnqueue;

			// Check to see if the last-known published sequence number has advanced
			// while we were enqueuing the awaiters. Need to use seq_cst memory order
			// here to ensure that if there are concurrent calls to publish() that would
			// wake up any of the awaiters we just enqueued that either we will see their
			// write to m_published slots or they will see our write to m_awaiters.
			//
			// Note also, that we are assuming that the last-known published sequence is
			// not going to advance more than buffer_size() ahead of targetSequence since
			// there is at least one consumer that won't be resumed and so thus can't
			// publish the sequence number it's waiting for to its sequence_barrier and so
			// producers won't be able to claim its slot in the buffer.
			//
			// TODO: Check whether we can weaken the memory order here to just use 'seq_cst' on the
			// first .load() and then use 'acquire' on subsequent .load().
			while (m_published[(lastKnownPublished + 1) & sequenceMask].load(std::memory_order_seq_cst) == (lastKnownPublished + 1))
			{
				++lastKnownPublished;
			}

			if (!TRAITS::precedes(lastKnownPublished, targetSequence))
			{
				// At least one awaiter we just enqueued has now been satisified.
				// To ensure it is woken up we need to reacquire the list of awaiters and resume
				awaiter_t* awaiters = m_awaiters.exchange(nullptr, std::memory_order_acquire);

				using diff_t = typename TRAITS::difference_type;

				diff_t minDiff = std::numeric_limits<diff_t>::max();

				while (awaiters != nullptr)
				{
					diff_t diff = TRAITS::difference(targetSequence, lastKnownPublished);
					if (diff > 0)
					{
						// Not yet ready.
						minDiff = diff < minDiff ? diff : minDiff;
						*awaitersToEnqueueTail = awaiters;
						awaitersToEnqueueTail = &awaiters->m_next;
						awaiters->m_lastKnownPublished = lastKnownPublished;
					}
					else
					{
						// Now ready.
						*awaitersToResumeTail = awaiters;
						awaitersToResumeTail = &awaiters->m_next;
					}
					awaiters = awaiters->m_next;
				}

				// Calculate the earliest sequence number that any awaiters in the
				// awaitersToEnqueue list are waiting for. We'll use this next time
				// around the loop.
				targetSequence = static_cast<SEQUENCE>(lastKnownPublished + minDiff);
			}

			// Null-terminate list of awaiters to enqueue.
			*awaitersToEnqueueTail = nullptr;

		} while (awaitersToEnqueue != nullptr);

		// Null-terminate awaiters to resume.
		*awaitersToResumeTail = nullptr;

		// Finally, resume any awaiters we've found that are ready to go.
		while (awaitersToResume != nullptr)
		{
			// Read m_next before calling .resume() as resuming could destroy the awaiter.
			awaiter_t* next = awaitersToResume->m_next;
			awaitersToResume->resume(lastKnownPublished);
			awaitersToResume = next;
		}
	}
}


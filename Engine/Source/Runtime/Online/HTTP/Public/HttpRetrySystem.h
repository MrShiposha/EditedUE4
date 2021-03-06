// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/Atomic.h"
#include "Interfaces/IHttpRequest.h"
#include "HttpRequestAdapter.h"

/**
 * Helpers of various types for the retry system
 */
namespace FHttpRetrySystem
{
    typedef uint32 RetryLimitCountType;
    typedef double RetryTimeoutRelativeSecondsType;

    inline RetryLimitCountType             RetryLimitCount(uint32 Value)             { return Value; }
    inline RetryTimeoutRelativeSecondsType RetryTimeoutRelativeSeconds(double Value) { return Value; }

    template <typename  IntrinsicType>
    IntrinsicType TZero();

    template <> inline float                           TZero<float>()                           { return 0.0f; }
    template <> inline RetryLimitCountType             TZero<RetryLimitCountType>()             { return RetryLimitCount(0); }
    template <> inline RetryTimeoutRelativeSecondsType TZero<RetryTimeoutRelativeSecondsType>() { return RetryTimeoutRelativeSeconds(0.0); }

    /**
     * TOptionalSetting merges a bool and an intrinsic value to remove the need
     * for having special values to indicate if the option is valid
     */
    template <typename IntrinsicType>
    struct TOptionalSetting
    {
        TOptionalSetting() : bUseValue(false), Value(TZero<IntrinsicType>())                          {}
        explicit TOptionalSetting(IntrinsicType InValue) : bUseValue(true), Value(InValue)                     {}
        TOptionalSetting(const TOptionalSetting& InValue) : bUseValue(InValue.bUseValue), Value(InValue.Value) {}

        static TOptionalSetting Unused()                       { return TOptionalSetting(); }
        static TOptionalSetting Create(IntrinsicType InValue)  { return TOptionalSetting(InValue); }

        bool            bUseValue;
        IntrinsicType   Value;
    };

    typedef TOptionalSetting<float>                           FRandomFailureRateSetting;
    typedef TOptionalSetting<RetryLimitCountType>             FRetryLimitCountSetting;
    typedef TOptionalSetting<RetryTimeoutRelativeSecondsType> FRetryTimeoutRelativeSecondsSetting;
	typedef TSet<int32> FRetryResponseCodes;
    typedef TSet<FName> FRetryVerbs;
	
	struct FRetryDomains
	{
		FRetryDomains(TArray<FString>&& InDomains) 
			: Domains(MoveTemp(InDomains))
			, ActiveIndex(0)
		{}

		/** The domains to use */
		const TArray<FString> Domains;
		/**
		 * Index into Domains to attempt
		 * Domains are cycled through on some errors, and when we succeed on one domain, we remain on that domain until that domain results in an error
		 */
		TAtomic<int32> ActiveIndex;
	};
	typedef TSharedPtr<FRetryDomains, ESPMode::ThreadSafe> FRetryDomainsPtr;
};

/**
* Delegate called when an Http request will be retried in the future
*
* @param first parameter - original Http request that started things
* @param second parameter - response received from the server if a successful connection was established
* @param third parameter - seconds in the future when the response will be retried
*/
DECLARE_DELEGATE_ThreeParams(FHttpRequestWillRetryDelegate, FHttpRequestPtr, FHttpResponsePtr, float);

namespace FHttpRetrySystem
{
    /**
     * class FRequest is what the retry system accepts as inputs
     */
    class FRequest 
		: public FHttpRequestAdapterBase
    {
    public:
        struct EStatus
        {
            enum Type
            {
                NotStarted = 0,
                Processing,
                ProcessingLockout,
                Cancelled,
                FailedRetry,
                FailedTimeout,
                Succeeded
            };
        };

    public:
		// IHttpRequest interface
		HTTP_API virtual bool ProcessRequest() override;
		HTTP_API virtual void CancelRequest() override;
		virtual FHttpRequestWillRetryDelegate& OnRequestWillRetry() { return OnRequestWillRetryDelegate; }
		
		// FRequest
		EStatus::Type GetRetryStatus() const { return Status; }

    protected:
		friend class FManager;

		HTTP_API FRequest(
			class FManager& InManager,
			const TSharedRef<IHttpRequest>& HttpRequest,
			const FRetryLimitCountSetting& InRetryLimitCountOverride = FRetryLimitCountSetting::Unused(),
			const FRetryTimeoutRelativeSecondsSetting& InRetryTimeoutRelativeSecondsOverride = FRetryTimeoutRelativeSecondsSetting::Unused(),
            const FRetryResponseCodes& InRetryResponseCodes = FRetryResponseCodes(),
            const FRetryVerbs& InRetryVerbs = FRetryVerbs(),
			const FRetryDomainsPtr& InRetryDomains = FRetryDomainsPtr()
			);

		void HttpOnRequestProgress(FHttpRequestPtr InHttpRequest, int32 BytesSent, int32 BytesRcv);

		/** Update our HTTP request's URL's domain from our RetryDomains */
		void SetUrlFromRetryDomains();
		/** Move to the next retry domain from our RetryDomains */
		void MoveToNextRetryDomain();

		EStatus::Type                        Status;

        FRetryLimitCountSetting              RetryLimitCountOverride;
        FRetryTimeoutRelativeSecondsSetting  RetryTimeoutRelativeSecondsOverride;
		FRetryResponseCodes					 RetryResponseCodes;
        FRetryVerbs                          RetryVerbs;
		FRetryDomainsPtr					 RetryDomains;
		/** The current index in RetryDomains we are attempting */
		int32								 RetryDomainsIndex = 0;
		/** The original URL before replacing anything from RetryDomains */
		FString								 OriginalUrl;

		FHttpRequestWillRetryDelegate OnRequestWillRetryDelegate;

		FManager& RetryManager;
    };
}

namespace FHttpRetrySystem
{
    class FManager
    {
    public:
        // FManager
		HTTP_API FManager(const FRetryLimitCountSetting& InRetryLimitCountDefault, const FRetryTimeoutRelativeSecondsSetting& InRetryTimeoutRelativeSecondsDefault);

		/**
		 * Create a new http request with retries
		 */
		HTTP_API TSharedRef<class FHttpRetrySystem::FRequest> CreateRequest(
			const FRetryLimitCountSetting& InRetryLimitCountOverride = FRetryLimitCountSetting::Unused(),
			const FRetryTimeoutRelativeSecondsSetting& InRetryTimeoutRelativeSecondsOverride = FRetryTimeoutRelativeSecondsSetting::Unused(),
			const FRetryResponseCodes& InRetryResponseCodes = FRetryResponseCodes(),
			const FRetryVerbs& InRetryVerbs = FRetryVerbs(),
			const FRetryDomainsPtr& InRetryDomains = FRetryDomainsPtr()
			);


        /**
         * Updates the entries in the list of retry requests. Optional parameters are for future connection health assessment
         *
         * @param FileCount       optional parameter that will be filled with the total files updated
         * @param FailingCount    optional parameter that will be filled with the total files that have are in a retrying state
         * @param FailedCount     optional parameter that will be filled with the total files that have failed
         * @param CompletedCount  optional parameter that will be filled with the total files that have completed
         *
         * @return                true if there are no failures or retries
         */
        HTTP_API bool Update(uint32* FileCount = NULL, uint32* FailingCount = NULL, uint32* FailedCount = NULL, uint32* CompletedCount = NULL);
		HTTP_API void SetRandomFailureRate(float Value) { RandomFailureRate = FRandomFailureRateSetting::Create(Value); }
		HTTP_API void SetDefaultRetryLimit(uint32 Value) { RetryLimitCountDefault = FRetryLimitCountSetting::Create(Value); }
		
		// @return Block the current process until all requests are flushed, or timeout has elapsed
		HTTP_API void BlockUntilFlushed(float TimeoutSec);

    protected:
		friend class FRequest;

        struct FHttpRetryRequestEntry
        {
            FHttpRetryRequestEntry(TSharedRef<FRequest>& InRequest);

            bool                    bShouldCancel;
            uint32                  CurrentRetryCount;
            double                  RequestStartTimeAbsoluteSeconds;
            double                  LockoutEndTimeAbsoluteSeconds;

            TSharedRef<FRequest>	Request;
        };

		bool ProcessRequest(TSharedRef<FRequest>& HttpRequest);
		void CancelRequest(TSharedRef<FRequest>& HttpRequest);

        // @return true if there is a no formal response to the request
        // @TODO return true if a variety of 5xx errors are the result of a formal response
        bool ShouldRetry(const FHttpRetryRequestEntry& HttpRetryRequestEntry);

        // @return true if retry chances have not been exhausted
        bool CanRetry(const FHttpRetryRequestEntry& HttpRetryRequestEntry);

        // @return true if the retry request has timed out
        bool HasTimedOut(const FHttpRetryRequestEntry& HttpRetryRequestEntry, const double NowAbsoluteSeconds);

        // @return number of seconds to lockout for
        float GetLockoutPeriodSeconds(const FHttpRetryRequestEntry& HttpRetryRequestEntry);

        // Default configuration for the retry system
        FRandomFailureRateSetting            RandomFailureRate;
        FRetryLimitCountSetting              RetryLimitCountDefault;
        FRetryTimeoutRelativeSecondsSetting  RetryTimeoutRelativeSecondsDefault;

        TArray<FHttpRetryRequestEntry>        RequestList;
    };
}

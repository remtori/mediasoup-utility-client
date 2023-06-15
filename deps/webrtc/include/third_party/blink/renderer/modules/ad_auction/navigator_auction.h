// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AD_AUCTION_NAVIGATOR_AUCTION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AD_AUCTION_NAVIGATOR_AUCTION_H_

#include <stdint.h>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/common/fenced_frame/redacted_fenced_frame_config.h"
#include "third_party/blink/public/mojom/interest_group/ad_auction_service.mojom-blink.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/ad_auction/join_leave_queue.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class AdRequestConfig;
class Ads;
class AuctionAdInterestGroup;
class AuctionAdConfig;
class ScopedAbortState;
class ScriptPromiseResolver;
class V8UnionFencedFrameConfigOrUSVString;

class MODULES_EXPORT NavigatorAuction final
    : public GarbageCollected<NavigatorAuction>,
      public Supplement<Navigator> {
 public:
  class AuctionHandle;
  static const char kSupplementName[];

  explicit NavigatorAuction(Navigator&);

  // Gets, or creates, NavigatorAuction supplement on Navigator.
  // See platform/Supplementable.h
  static NavigatorAuction& From(ExecutionContext*, Navigator&);

  ScriptPromise joinAdInterestGroup(ScriptState*,
                                    const AuctionAdInterestGroup*,
                                    double,
                                    ExceptionState&);
  static ScriptPromise joinAdInterestGroup(ScriptState*,
                                           Navigator&,
                                           const AuctionAdInterestGroup*,
                                           double,
                                           ExceptionState&);
  ScriptPromise leaveAdInterestGroup(ScriptState*,
                                     const AuctionAdInterestGroup*,
                                     ExceptionState&);
  static ScriptPromise leaveAdInterestGroup(ScriptState*,
                                            Navigator&,
                                            const AuctionAdInterestGroup*,
                                            ExceptionState&);
  // Implicit leaveAdInterestGroup - only supported when called from within
  // a fenced frame showing FLEDGE ads.
  ScriptPromise leaveAdInterestGroupForDocument(ScriptState*, ExceptionState&);
  static ScriptPromise leaveAdInterestGroup(ScriptState*,
                                            Navigator&,
                                            ExceptionState&);

  void updateAdInterestGroups();
  static void updateAdInterestGroups(ScriptState*, Navigator&, ExceptionState&);
  ScriptPromise runAdAuction(ScriptState*,
                             const AuctionAdConfig*,
                             ExceptionState&);
  static ScriptPromise runAdAuction(ScriptState*,
                                    Navigator&,
                                    const AuctionAdConfig*,
                                    ExceptionState&);

  // If called from a FencedFrame that was navigated to the URN resulting from
  // an interest group ad auction, returns a Vector of ad component URNs
  // associated with the winning bid in that auction.
  //
  // `num_ad_components` is the number of ad component URNs to put in the
  // Vector. To avoid leaking data from the winning bidder worklet, the number
  // of ad components in the winning bid is not exposed. Instead, it's padded
  // with URNs to length kMaxAdAuctionAdComponents, and calling this method
  // returns the first `num_ad_components` URNs.
  //
  // Throws an exception if `num_ad_components` is greater than
  // kMaxAdAuctionAdComponents, or if called from a frame that was not navigated
  // to a URN representing the winner of an ad auction.
  static Vector<String> adAuctionComponents(ScriptState* script_state,
                                            Navigator& navigator,
                                            uint16_t num_ad_components,
                                            ExceptionState& exception_state);

  ScriptPromise deprecatedURNToURL(ScriptState* script_state,
                                   const String& urn_uuid,
                                   bool send_reports,
                                   ExceptionState& exception_state);

  static ScriptPromise deprecatedURNToURL(
      ScriptState* script_state,
      Navigator& navigator,
      const V8UnionFencedFrameConfigOrUSVString* urn_or_config,
      bool send_reports,
      ExceptionState& exception_state);

  ScriptPromise deprecatedReplaceInURN(
      ScriptState* script_state,
      const String& urn_uuid,
      const Vector<std::pair<String, String>>& replacement,
      ExceptionState& exception_state);

  static ScriptPromise deprecatedReplaceInURN(
      ScriptState* script_state,
      Navigator& navigator,
      const V8UnionFencedFrameConfigOrUSVString* urn_or_config,
      const Vector<std::pair<String, String>>& replacement,
      ExceptionState& exception_state);

  ScriptPromise createAdRequest(ScriptState*,
                                const AdRequestConfig*,
                                ExceptionState&);
  static ScriptPromise createAdRequest(ScriptState*,
                                       Navigator&,
                                       const AdRequestConfig*,
                                       ExceptionState&);
  ScriptPromise finalizeAd(ScriptState*,
                           const Ads*,
                           const AuctionAdConfig*,
                           ExceptionState&);
  static ScriptPromise finalizeAd(ScriptState*,
                                  Navigator&,
                                  const Ads*,
                                  const AuctionAdConfig*,
                                  ExceptionState&);

  void Trace(Visitor* visitor) const override {
    visitor->Trace(ad_auction_service_);
    Supplement<Navigator>::Trace(visitor);
  }

 private:
  // Pending cross-site interest group joins and leaves. These may be added to a
  // queue before being passed to the browser process.

  struct PendingJoin {
    mojom::blink::InterestGroupPtr interest_group;
    mojom::blink::AdAuctionService::JoinInterestGroupCallback callback;
  };

  struct PendingLeave {
    scoped_refptr<const SecurityOrigin> owner;
    String name;
    mojom::blink::AdAuctionService::LeaveInterestGroupCallback callback;
  };

  // Tells the browser process to start `pending_join`. Its callback will be
  // invoked on completion.
  void StartJoin(PendingJoin&& pending_join);

  // Completion callback for joinInterestGroup() Mojo calls.
  void JoinComplete(bool is_cross_origin,
                    ScriptPromiseResolver* resolver,
                    bool failed_well_known_check);

  // Tells the browser process to start `pending_leave`. Its callback will be
  // invoked on completion.
  void StartLeave(PendingLeave&& pending_leave);

  // Completion callback for leaveInterestGroup() Mojo calls.
  void LeaveComplete(bool is_cross_origin,
                     ScriptPromiseResolver* resolver,
                     bool failed_well_known_check);

  // Completion callback for createAdRequest() Mojo call.
  void AdsRequested(ScriptPromiseResolver* resolver,
                    const WTF::String& ads_guid);
  // Completion callback for finalizeAd() Mojo call.
  void FinalizeAdComplete(ScriptPromiseResolver* resolver,
                          const absl::optional<KURL>& creative_url);
  // Completion callback for Mojo call made by runAdAuction().
  void AuctionComplete(
      ScriptPromiseResolver*,
      std::unique_ptr<ScopedAbortState>,
      bool resolve_to_config,
      bool manually_aborted,
      const absl::optional<FencedFrame::RedactedFencedFrameConfig>&);
  // Completion callback for Mojo call made by deprecatedURNToURL().
  void GetURLFromURNComplete(ScriptPromiseResolver*,
                             const absl::optional<KURL>&);
  // Completion callback for Mojo call made by deprecatedReplaceInURNComplete().
  void ReplaceInURNComplete(ScriptPromiseResolver* resolver);

  // Manage queues of cross-site join and leave operations that have yet to be
  // sent to the browser process.
  JoinLeaveQueue<PendingJoin> queued_cross_site_joins_;
  JoinLeaveQueue<PendingLeave> queued_cross_site_leaves_;

  HeapMojoRemote<mojom::blink::AdAuctionService> ad_auction_service_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AD_AUCTION_NAVIGATOR_AUCTION_H_

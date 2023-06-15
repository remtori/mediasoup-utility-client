// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_FENCED_FRAME_CONFIG_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_FENCED_FRAME_CONFIG_H_

#include "base/notreached.h"
#include "base/types/pass_key.h"
#include "third_party/blink/public/common/fenced_frame/redacted_fenced_frame_config.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_html_fenced_frame_element.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_opaqueproperty_unsignedlong.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_opaqueproperty_usvstring.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

// For base::PassKey<NavigationAuction>.
// TODO(crbug.com/1347953): Remove when the accessor is removed.
class NavigatorAuction;

// FencedFrameConfig class implements the FencedFrameConfig IDL. It specifies
// the fenced frame's inner properties. It can be returned by shared storage's
// selectURL() and FLEDGE's runAdAuction(), or directly constructed for
// navigation to non-opaque URLs. Please see the link for examples of installing
// FencedFrameConfig in fenced frames.
// https://github.com/WICG/fenced-frame/issues/48#issuecomment-1245809738.
class CORE_EXPORT FencedFrameConfig final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum class Attribute {
    kURL,
    kWidth,
    kHeight,
  };

  // Note this visibility has different semantics from
  // FencedFrameURLMapping::VisibilityToEmbedder and
  // FencedFrameURLMapping::VisibilityToContent. Here `AttributeVisibility`
  // specifies whether each attribute is transparent to the author, or is null.
  // Whereas the enums in FencedFrameURLMapping specify whether information
  // should be redacted when it is communicated to different entities
  // (renderers).
  enum class AttributeVisibility {
    kTransparent,
    kOpaque,
    kNull,
  };

  // Create an inner config with a given url, the url will be transparent.
  static FencedFrameConfig* Create(const String& url);

  static FencedFrameConfig* From(
      const FencedFrame::RedactedFencedFrameConfig& config);

  // Construct an inner config with a given url, the url will be transparent.
  explicit FencedFrameConfig(const String& url);

  // Construct an inner config given a redacted fenced frame config
  explicit FencedFrameConfig(
      const FencedFrame::RedactedFencedFrameConfig& config);

  FencedFrameConfig(const FencedFrameConfig&) = delete;
  FencedFrameConfig& operator=(const FencedFrameConfig&) = delete;

  V8UnionOpaquePropertyOrUSVString* url() const;
  V8UnionOpaquePropertyOrUnsignedLong* width() const;
  V8UnionOpaquePropertyOrUnsignedLong* height() const;

  // Get attribute's value ignoring visibility.
  template <Attribute attr>
  auto GetValueIgnoringVisibility() const {
    if constexpr (attr == Attribute::kURL) {
      return url_;
    } else if constexpr (attr == Attribute::kWidth) {
      return width_;
    } else if constexpr (attr == Attribute::kHeight) {
      return height_;
    }
    NOTREACHED();
  }

  absl::optional<KURL> urn_uuid(base::PassKey<HTMLFencedFrameElement>) {
    return urn_uuid_;
  }

  // Temporary accessor for `deprecatedURNToURL` and `deprecatedReplaceInURN`.
  // TODO(crbug.com/1347953): Remove when those functions are removed.
  absl::optional<KURL> urn_uuid(base::PassKey<NavigatorAuction>) {
    return urn_uuid_;
  }

  absl::optional<gfx::Size> content_size(
      base::PassKey<HTMLFencedFrameElement>) {
    return content_size_;
  }

  bool deprecated_should_freeze_initial_size(
      base::PassKey<HTMLFencedFrameElement>) {
    return deprecated_should_freeze_initial_size_;
  }

 private:
  KURL url_;
  uint32_t width_;
  uint32_t height_;

  AttributeVisibility url_attribute_visibility_ = AttributeVisibility::kNull;
  AttributeVisibility size_attribute_visibility_ = AttributeVisibility::kNull;

  // Attribute's union type based on its value type.
  template <typename T>
  struct AttributeUnion;

  // Get attribute's visibility.
  template <Attribute attr>
  AttributeVisibility GetAttributeVisibility() const {
    switch (attr) {
      case Attribute::kURL:
        return url_attribute_visibility_;
      case Attribute::kWidth:
      case Attribute::kHeight:
        return size_attribute_visibility_;
    }
    NOTREACHED();
  }

  // Get attribute's value.
  template <Attribute attr>
  auto GetValue() const {
    if constexpr (attr == Attribute::kURL) {
      return url_.GetString();
    } else if constexpr (attr == Attribute::kWidth) {
      return width_;
    } else if constexpr (attr == Attribute::kHeight) {
      return height_;
    }
    NOTREACHED();
  }

  // Get the union based on attribute's `AttributeType`.
  template <Attribute attr>
  auto Get() const ->
      typename AttributeUnion<decltype(GetValue<attr>())>::Type* {
    auto attribute_visibility = GetAttributeVisibility<attr>();
    auto value = GetValue<attr>();

    using UnionType = typename AttributeUnion<decltype(value)>::Type;

    switch (attribute_visibility) {
      case AttributeVisibility::kTransparent:
        return MakeGarbageCollected<UnionType>(value);
      case AttributeVisibility::kOpaque:
        return MakeGarbageCollected<UnionType>(
            V8OpaqueProperty(V8OpaqueProperty::Enum::kOpaque));
      case AttributeVisibility::kNull:
        return nullptr;
    }
    NOTREACHED();
  }

  // The URN attribute is used as the key in the FencedFrameURNMapping map. When
  // we navigate a fenced frame using a `FencedFrameConfig` object that has a
  // non-null `urn_`, we navigate to that URN instead of the platform-provided
  // URL. This value is never exposed to the web platform.
  absl::optional<KURL> urn_uuid_;

  // `content_size` and `deprecated_should_freeze_initial_size` temporarily need
  // to be treated differently than other fields, because for implementation
  // convenience the fenced frame size is frozen by the embedder. In the long
  // term, it should be frozen by the browser (i.e. neither the embedder's
  // renderer nor the fenced frame's renderer), so that it is secure to
  // compromised renderers.

  // The size that the inner frame of the fenced frame should be frozen to (if
  // any).
  absl::optional<gfx::Size> content_size_;

  // Whether we should use the old size freezing behavior (coerce the size at
  // navigation time to an allowlist, then freeze it) for backwards
  // compatibility.
  bool deprecated_should_freeze_initial_size_ = false;
};

template <>
struct FencedFrameConfig::AttributeUnion<String> {
  using Type = V8UnionOpaquePropertyOrUSVString;
};
template <>
struct FencedFrameConfig::AttributeUnion<uint32_t> {
  using Type = V8UnionOpaquePropertyOrUnsignedLong;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_FENCED_FRAME_CONFIG_H_

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_COMMON_PERMISSIONS_POLICY_PERMISSIONS_POLICY_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_COMMON_PERMISSIONS_POLICY_PERMISSIONS_POLICY_MOJOM_TRAITS_H_

#include <map>

#include "mojo/public/cpp/bindings/enum_traits.h"
#include "third_party/blink/common/permissions_policy/policy_value_mojom_traits.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-shared.h"
#include "url/mojom/origin_mojom_traits.h"

namespace mojo {

template <>
class BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::OriginWithPossibleWildcardsDataView,
                 blink::OriginWithPossibleWildcards> {
 public:
  static const url::Origin& origin(const blink::OriginWithPossibleWildcards&
                                       origin_with_possible_wildcards) {
    return origin_with_possible_wildcards.origin;
  }
  static bool has_subdomain_wildcard(const blink::OriginWithPossibleWildcards&
                                         origin_with_possible_wildcards) {
    return origin_with_possible_wildcards.has_subdomain_wildcard;
  }

  static bool Read(blink::mojom::OriginWithPossibleWildcardsDataView in,
                   blink::OriginWithPossibleWildcards* out);
};

template <>
class BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ParsedPermissionsPolicyDeclarationDataView,
                 blink::ParsedPermissionsPolicyDeclaration> {
 public:
  static blink::mojom::PermissionsPolicyFeature feature(
      const blink::ParsedPermissionsPolicyDeclaration& policy) {
    return policy.feature;
  }
  static const std::vector<blink::OriginWithPossibleWildcards>& allowed_origins(
      const blink::ParsedPermissionsPolicyDeclaration& policy) {
    return policy.allowed_origins;
  }
  static bool matches_all_origins(
      const blink::ParsedPermissionsPolicyDeclaration& policy) {
    return policy.matches_all_origins;
  }
  static bool matches_opaque_src(
      const blink::ParsedPermissionsPolicyDeclaration& policy) {
    return policy.matches_opaque_src;
  }

  static bool Read(blink::mojom::ParsedPermissionsPolicyDeclarationDataView in,
                   blink::ParsedPermissionsPolicyDeclaration* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_COMMON_PERMISSIONS_POLICY_PERMISSIONS_POLICY_MOJOM_TRAITS_H_

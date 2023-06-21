#!/bin/bash

set -Eeuo pipefail

cd "$(dirname "$0")"

cmake -GNinja -Bcmake-build-release
cd cmake-build-release
cmake --build .

ar -M <<EOF
CREATE librusty_msc.a
ADDLIB libmsc.a
ADDLIB deps/fmt/libfmt.a
ADDLIB deps/libmediasoupclient/libmediasoupclient.a
ADDLIB deps/libmediasoupclient/libsdptransform/libsdptransform.a
ADDLIB ../deps/webrtc/libwebrtc.a
SAVE
END
EOF

cp librusty_msc.a ../deps

cd ..
bindgen \
    --no-layout-tests \
    --no-derive-debug \
    --no-derive-copy \
    --merge-extern-blocks \
    --raw-line '#![allow(non_upper_case_globals)]' \
    msc/lib.h -o src/ffi.rs \
    -- \
    -DGEN_BINDINGS

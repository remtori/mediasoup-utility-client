#!/bin/bash

set -Eeuo pipefail

cd "$(dirname "$0")"

cmake -GNinja -Bbuild
cd build
cmake --build .

ar -M <<EOF
CREATE librusty_msc.a
ADDLIB libmsc.a
ADDLIB deps/libmediasoupclient/libfmt.a
ADDLIB deps/libmediasoupclient/libmediasoupclient.a
ADDLIB deps/libmediasoupclient/libsdptransform/libsdptransform.a
ADDLIB ../deps/webrtc/libwebrtc.a
SAVE
END
EOF

cp librusty_msc.a ../deps

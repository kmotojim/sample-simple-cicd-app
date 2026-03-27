# =============================================================================
# Hello Server - Container Build
#
# エアギャップ環境向け: <MIRROR_REGISTRY> を実際のミラーレジストリに
# 置き換えてください（例: registry.example.com）
# =============================================================================

# --- Builder stage ---
# gcc-c++, cmake, make がプリインストールされた DevSpaces UDI イメージ
FROM registry.redhat.io/devspaces/udi-rhel9:latest AS builder

WORKDIR /build

COPY CMakeLists.txt .
COPY src/ src/

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++ -static-libgcc" && \
    cmake --build build --parallel $(nproc)

# --- Runtime stage ---
FROM registry.redhat.io/ubi9/ubi-minimal:latest

COPY --from=builder /build/build/hello-server /usr/local/bin/hello-server

EXPOSE 8080

USER 1001

CMD ["hello-server"]

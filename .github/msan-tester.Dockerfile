FROM ubuntu:16.04

WORKDIR /usr/src
ENV CC=clang
ENV CXX=clang++

# Install tools needed
RUN apt-get update && apt-get install -y \
  cmake \
  git \
  clang \
  libc++-dev \
  pkg-config \
  && ln -s /usr/bin/llvm-symbolizer-3.8 /usr/bin/llvm-symbolizer \
  && rm -rf /var/lib/apt/lists/*

# Build libcxx with MSAN
RUN git clone --depth 1 https://github.com/llvm/llvm-project.git \
	&& cd llvm-project \
	&& cp -r libcxx llvm/projects/ \
	&& cp -r libcxxabi llvm/projects/ \
	&& mkdir /usr/src/libcxx_msan \
	&& cd /usr/src/libcxx_msan \
	&& cmake ../llvm-project/llvm -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DLLVM_USE_SANITIZER=Memory \
	&& make cxx -j4 \
	&& rm -rf /usr/src/llvm-project

ENV MSAN_CFLAGS="-fsanitize=memory -stdlib=libc++ -L/usr/src/libcxx_msan/lib -lc++abi -I/usr/src/libcxx_msan/include -I/usr/src/libcxx_msan/include/c++/v1"

# Install gtest globally
RUN git clone --depth 1 https://github.com/google/googletest.git -b release-1.8.0 \
	&& mkdir googletest/build \
	&& cd googletest/build \
	&& cmake ../googletest -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_FLAGS="$MSAN_CFLAGS" -DCMAKE_CXX_FLAGS="$MSAN_CFLAGS" \
	&& make -j4 \
	&& make install \
	&& rm -rf /usr/src/googletest

ENV CFLAGS="-fno-omit-frame-pointer -O0 -g -fsanitize=memory"
ENV LDFLAGS="-fsanitize=memory"

# OpenSSL
RUN git clone --depth 1 https://github.com/openssl/openssl.git -b OpenSSL_1_1_0l \
	&& cd openssl \
	&& CC="$CC $CFLAGS" ./config no-asm \
	&& make install \
	&& rm -rf /usr/src/openssl

# jsmn
RUN git clone --depth 1 https://github.com/zserge/jsmn.git -b v1.0.0 \
	&& cd jsmn \
	&& CFLAGS="$CFLAGS -fPIC" make \
	&& cp libjsmn.a /usr/local/lib \
	&& cp jsmn.h /usr/local/include \
	&& rm -rf /usr/src/jsmn

# libsrtp
RUN git clone --depth 1 https://github.com/cisco/libsrtp.git -b v2.2.0 \
	&& cd libsrtp \
	&& ./configure --enable-openssl --with-openssl-dir=/usr/local \
	&& make \
	&& make install \
	&& rm -rf /usr/src/libsrtp


# libwebsockets
RUN git clone https://github.com/warmcat/libwebsockets.git \
	&& cd libwebsockets \
	&& git reset --hard  3179323afa81448287a15982755ed0e4a34d80cb \
	&& mkdir build \
	&& cd build \
	&& cmake .. -DLWS_WITH_HTTP2=1         \
	   -DLWS_HAVE_HMAC_CTX_new=1           \
	   -DLWS_HAVE_SSL_EXTRA_CHAIN_CERTS=1  \
	   -DLWS_HAVE_OPENSSL_ECDH_H=1         \
	   -DLWS_HAVE_EVP_MD_CTX_free=1        \
	   -DLWS_WITHOUT_SERVER=1              \
	   -DLWS_WITHOUT_TESTAPPS=1            \
	   -DLWS_WITH_THREADPOOL=1             \
	   -DLWS_WITHOUT_TEST_SERVER_EXTPOLL=1 \
	   -DLWS_WITHOUT_TEST_PING=1 	       \
	   -DLWS_WITHOUT_TEST_CLIENT=1         \
	   -DLWS_WITH_SHARED=1                 \
	   -DLWS_STATIC_PIC=1                  \
	   -DLWS_WITH_ZLIB=0                   \
	   -DOPENSSL_ROOT_DIR=/usr/local       \
	&& make \
	&& make install \
	&& rm -rf /usr/src/libwebsockets

# usrsctp
RUN git clone https://github.com/sctplab/usrsctp.git \
	&& cd usrsctp \
	&& git reset --hard  913de3599feded8322882bdae69f346da5a258fc \
	&& mkdir build \
	&& cd build \
	&& cmake .. -Dsctp_werror=0 \
	&& make \
	&& make install \
	&& rm -rf /usr/src/usrsctp


ENV LD_LIBRARY_PATH='/usr/src/libcxx_msan/lib/'

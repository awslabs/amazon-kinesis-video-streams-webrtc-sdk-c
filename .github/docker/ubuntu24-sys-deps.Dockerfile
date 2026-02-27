FROM public.ecr.aws/ubuntu/ubuntu:24.04_stable

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    gdb \
    libssl-dev \
    libsrtp2-dev \
    libusrsctp-dev \
    libgtest-dev \
    && rm -rf /var/lib/apt/lists/*

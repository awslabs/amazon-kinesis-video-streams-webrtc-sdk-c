FROM public.ecr.aws/ubuntu/ubuntu:24.04_stable

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    clang \
    git \
    perl \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

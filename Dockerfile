FROM ubuntu:20.04

COPY . .

RUN apt-get -y update && apt-get -y upgrade

RUN apt-get install -y clang-8


# RUN ls -l /usr/bin/cc /usr/bin/c++ /usr/bin/clang /usr/bin/clang++

# RUN ln -sf /usr/bin/clang /usr/bin/cc
# RUN ln -sf /usr/bin/clang++ /usr/bin/c++

# RUN update-alternatives --install /usr/bin/cc cc /usr/bin/clang
# RUN update-alternatives --install /usr/bin/c++ c++ /usr/bin/clang++

RUN clang-8 -g -O1 -fsanitize=fuzzer,address -o fuzz_dr_flac /tests/flac/fuzz_dr_flac.c

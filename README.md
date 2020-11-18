# mrpc
基于c++20特性实现的rpc框架。

The project need llvm, clang and libc++ of llvm for linux
On ubuntu, you can do this:

WSL ubuntu:
sudo cp /etc/apt/sources.list /etc/apt/sources.list.bak
vim /etc/apt/sources.list
  :%s/security.ubuntu/mirrors.aliyun/g
  :%s/archive.ubuntu/mirrors.aliyun/g
sudo apt update

sudo apt-get install git cmake ninja-build clang lld

mkdir llvm
cd llvm
git clone --depth=1 https://github.com/llvm-mirror/llvm.git llvm
git clone --depth=1 https://github.com/llvm-mirror/clang.git llvm/tools/clang
git clone --depth=1 https://github.com/llvm-mirror/lld.git llvm/tools/lld
git clone --depth=1 https://github.com/llvm-mirror/libcxx.git llvm/projects/libcxx
git clone --depth=1 https://github.com/llvm-mirror/libcxxabi.git llvm/projects/libcxxabi
ln -s llvm/tools/clang clang
ln -s llvm/tools/lld lld
ln -s llvm/projects/libcxx libcxx
ln -s llvm/projects/libcxxabi libcxxabi

mkdir clang-build
cd clang-build
cmake -GNinja \
      -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
      -DCMAKE_C_COMPILER=/usr/bin/clang \
      -DCMAKE_BUILD_TYPE=MinSizeRel \
      -DCMAKE_INSTALL_PREFIX="/usr" \
      -DCMAKE_BUILD_WITH_INSTALL_RPATH="yes" \
      -DLLVM_TARGETS_TO_BUILD=X86 \
      -DLLVM_ENABLE_PROJECTS="lld;clang" \
      ../llvm
sudo ninja install-clang \
      install-clang-headers \
      install-llvm-ar \
      install-lld

mkdir libcxxabi-build
cd libcxxabi-build
cmake -GNinja \
      -DCMAKE_CXX_COMPILER="/usr/bin/clang++" \
      -DCMAKE_C_COMPILER="/usr/bin/clang" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="/usr" \
      -DLLVM_PATH="../llvm" \
      ../libcxxabi
ninja cxxabi
sudo ninja install

mkdir libcxx-build
cd libcxx-build
cmake -GNinja \
      -DCMAKE_CXX_COMPILER="/usr/bin/clang++" \
      -DCMAKE_C_COMPILER="/usr/bin/clang" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="/usr" \
      -DLLVM_PATH="../llvm" \
      -DLIBCXX_CXX_ABI=libstdc++ \
      -DLIBCXX_CXX_ABI_INCLUDE_PATHS="/usr/include/c++/9" \
      ../libcxx
ninja cxx
sudo ninja install

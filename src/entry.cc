//
// Created by dsugisawa on 2017/07/16.
//
//
#include "inc/nmmc.hpp"
#include "inc/nmmc.inl"
#include "inc/nmmc_process.hpp"

using namespace NMMC;

int main(int argc, char** argv) {
    LOG("start, netmap memcached.");
    Process    proc(argv[0]);
    // プロセスセットアップ
    proc.Setup(argc, argv);

    proc.Start();   // プロセス起動
    proc.Run();     // メインループ

    return 0;
}
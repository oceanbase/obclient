# CMake generated Testfile for 
# Source directory: /home/jianliang.yjl/project/server-10.4/storage/maria/unittest
# Build directory: /home/jianliang.yjl/project/server-10.4/storage/maria/unittest
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(ma_control_file "ma_control_file-t")
add_test(trnman "trnman-t")
add_test(ma_test_loghandler "ma_test_loghandler-t")
add_test(ma_test_loghandler_multigroup "ma_test_loghandler_multigroup-t")
add_test(ma_test_loghandler_multithread "ma_test_loghandler_multithread-t")
add_test(ma_test_loghandler_pagecache "ma_test_loghandler_pagecache-t")
add_test(ma_test_loghandler_long "ma_test_loghandler_long-t")
add_test(ma_test_loghandler_noflush "ma_test_loghandler_noflush-t")
add_test(ma_test_loghandler_first_lsn "ma_test_loghandler_first_lsn-t")
add_test(ma_test_loghandler_max_lsn "ma_test_loghandler_max_lsn-t")
add_test(ma_test_loghandler_purge "ma_test_loghandler_purge-t")
add_test(ma_test_loghandler_readonly "ma_test_loghandler_readonly-t")
add_test(ma_test_loghandler_nologs "ma_test_loghandler_nologs-t")
add_test(ma_pagecache_single_1k "ma_pagecache_single_1k-t")
add_test(ma_pagecache_single_8k "ma_pagecache_single_8k-t")
add_test(ma_pagecache_single_64k "ma_pagecache_single_64k-t")
add_test(ma_pagecache_consist_1k "ma_pagecache_consist_1k-t")
add_test(ma_pagecache_consist_64k "ma_pagecache_consist_64k-t")
add_test(ma_pagecache_consist_1kHC "ma_pagecache_consist_1kHC-t")
add_test(ma_pagecache_consist_64kHC "ma_pagecache_consist_64kHC-t")
add_test(ma_pagecache_consist_1kRD "ma_pagecache_consist_1kRD-t")
add_test(ma_pagecache_consist_64kRD "ma_pagecache_consist_64kRD-t")
add_test(ma_pagecache_consist_1kWR "ma_pagecache_consist_1kWR-t")
add_test(ma_pagecache_consist_64kWR "ma_pagecache_consist_64kWR-t")
add_test(ma_pagecache_rwconsist_1k "ma_pagecache_rwconsist_1k-t")
add_test(ma_pagecache_rwconsist2_1k "ma_pagecache_rwconsist2_1k-t")

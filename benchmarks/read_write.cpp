#include <ctime>
#include <iostream>
#include <cstdlib>

#include <nkvdb.h>
#include <logger.h>

#include <boost/program_options.hpp>

namespace po = boost::program_options;

const std::string storage_path = "benchmarkStorage";

size_t meas2write = 10;
size_t pagesize = 1000000;
bool write_only = false;
bool verbose = false;
bool dont_remove = false;
bool enable_dyn_cache = false;
size_t cache_size=nkvdb::defaultcacheSize;
size_t cache_pool_size=nkvdb::defaultcachePoolSize;

void makeAndWrite(int mc, int ic) {
  logger("makeAndWrite mc:" << mc << " ic:" << ic << " dyn_cache: " << (enable_dyn_cache ? "true" : "false"));

  const uint64_t storage_size =
      sizeof(nkvdb::Page::Header) + (sizeof(nkvdb::Meas) * pagesize);

  nkvdb::Storage::Storage_ptr ds =
      nkvdb::Storage::Create(storage_path, storage_size);

  ds->enableCacheDynamicSize(enable_dyn_cache);
  ds->setPoolSize(cache_pool_size);
  ds->setCacheSize(cache_size);
  
  clock_t write_t0 = clock();
  nkvdb::Meas meas = nkvdb::Meas::empty();

  for (int i = 0; i < ic; ++i) {
    meas.value = i % mc;
    meas.id = i % mc;
    meas.source = meas.flag = i % mc;
    meas.time = i;

    ds->append(meas);
  }

  clock_t write_t1 = clock();
  logger("write time: " << ((float)write_t1 - write_t0) / CLOCKS_PER_SEC);
  ds->Close();
  ds = nullptr;
  utils::rm(storage_path);
}

void readIntervalBench(nkvdb::Storage::Storage_ptr ds,   nkvdb::Time from, nkvdb::Time to,   std::string message) {

  clock_t read_t0 = clock();
  nkvdb::Meas::MeasList meases {};
  auto reader = ds->readInterval(from, to);
  reader->readAll(&meases);
  clock_t read_t1 = clock();

  logger("=> : " << message << " time: " << ((float)read_t1 - read_t0) / CLOCKS_PER_SEC);
}

void readIntervalBenchFltr(nkvdb::IdArray ids, nkvdb::Flag src,
                           nkvdb::Flag flag,
                           nkvdb::Storage::Storage_ptr ds,
                           nkvdb::Time from, nkvdb::Time to,
                           std::string message) {
  clock_t read_t0 = clock();
  
  nkvdb::Meas::MeasList output;
  auto reader = ds->readInterval(ids, src, flag, from, to);
  reader->readAll(&output);
  clock_t read_t1 = clock();

  logger("=> :" << message << " time: " << ((float)read_t1 - read_t0) / CLOCKS_PER_SEC);
}

int main(int argc, char *argv[]) {
  po::options_description desc("IO benchmark.\n Allowed options");
  desc.add_options()("help", "produce help message")(
      "mc", po::value<size_t>(&meas2write)->default_value(meas2write), "measurment count")
      ("dyncache",po::value<bool>(&enable_dyn_cache)->default_value(enable_dyn_cache),"enable dynamic cache")
      ("cache-size",po::value<size_t>(&cache_size)->default_value(cache_size),"cache size")
      ("cache-pool-size",po::value<size_t>(&cache_pool_size)->default_value(cache_pool_size),"cache pool size")
      ("page-size",po::value<size_t>(&pagesize)->default_value(pagesize),"page size")
      ("write-only", "don`t run readInterval")
      ("verbose", "verbose ouput")
      ("dont-remove", "dont remove created storage");

  po::variables_map vm;
  try {
    po::store(po::parse_command_line(argc, argv, desc), vm);
  } catch (std::exception &ex) {
    logger("Error: " << ex.what());
    exit(1);
  }
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return 1;
  }

  if (vm.count("write-only")) {
    write_only = true;
  }

  if (vm.count("verbose")) {
    verbose = true;
  }

  if (vm.count("dont-remove")) {
    dont_remove = true;
  }

  

  makeAndWrite(meas2write, 1000000);
  makeAndWrite(meas2write, 2000000);
  makeAndWrite(meas2write, 3000000);

  if (!write_only) {
	  int pagesize = 1000000;
	  const uint64_t storage_size =
          sizeof(nkvdb::Page::Header) + (sizeof(nkvdb::Meas) * pagesize);

    nkvdb::Storage::Storage_ptr ds = nkvdb::Storage::Create(storage_path, storage_size);
    nkvdb::Meas meas = nkvdb::Meas::empty();

    ds->enableCacheDynamicSize(enable_dyn_cache);
    ds->setPoolSize(cache_pool_size);
    ds->setCacheSize(cache_size);

    logger( "creating storage...");
    logger( "pages_size:" << pagesize);

    for (int64_t i = 0; i < pagesize * 10; ++i) {
      clock_t verb_t0 = clock();
	  
      meas.value = i;
      meas.id = i % meas2write;
      meas.source = meas.flag = i % meas2write;
      meas.time = i;
	  
      ds->append(meas);
      clock_t verb_t1 = clock();
      if (verbose) {
        logger("write[" << i << "]: " << ((float)verb_t1 - verb_t0) / CLOCKS_PER_SEC);
      }
    }

    logger( "big readers");
    readIntervalBench(ds, 0, pagesize / 2, "[0-0.5]");
    readIntervalBench(ds, 3 * pagesize + pagesize / 2, 3 * pagesize * 2,
                      "[3.5-6]");
    readIntervalBench(ds, 7 * pagesize, 8 * pagesize + pagesize * 1.5,
                      "[7-9.5]");

    logger("small readers");
    readIntervalBench(ds, 5 * pagesize + pagesize / 3, 6 * pagesize, "[5.3-6]");
    readIntervalBench(ds, 2 * pagesize, 2 * pagesize + pagesize * 1.5,
                      "[2-3.5]");
    readIntervalBench(ds, 6 * pagesize * 0.3, 7 * pagesize * 0.7, "[6.3-7.7]");

    logger("fltr big readers");
    readIntervalBenchFltr(nkvdb::IdArray{0, 1, 2, 3, 4, 5}, 1, 1, ds, 0,
                          pagesize / 2, "Id: {0- 5}, src:1, flag:1; [0-0.5]");

    readIntervalBenchFltr(nkvdb::IdArray{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}, 1, 0,
                          ds, 3 * pagesize + pagesize / 2, 3 * pagesize * 2,
                          "Id: {0- 9}, src:1, flag:0; [3.5-6]");

    readIntervalBenchFltr(
        nkvdb::IdArray{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}, 1, 1, ds,
        7 * pagesize, 8 * pagesize + pagesize * 1.5,
        "Id: {0-12}, src:1, flag:1; [7-9.5]");

    logger("fltr small readers");
    readIntervalBenchFltr(nkvdb::IdArray{0, 1}, 1, 1, ds,
                          5 * pagesize + pagesize / 3, 6 * pagesize,
                          "Id: {0,1},   src:1,  flag:1; [5.3-6.0]");
    readIntervalBenchFltr(nkvdb::IdArray{0, 1, 3}, 1, 1, ds, 2 * pagesize,
                          2 * pagesize + pagesize * 1.5,
                          "Id: {0,1,3}, src:1,  flag:1; [2.0-3.5]");
    readIntervalBenchFltr(nkvdb::IdArray{0}, 1, 1, ds, 6 * pagesize * 0.3,
                          7 * pagesize * 0.7,
                          "Id: {0},     src:1,  flag:1; [6.3-7.7]");
    ds->Close();

    if (!dont_remove)
      utils::rm(storage_path);
  }
}

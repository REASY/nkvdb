#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Main
#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>
#include "test_common.h"
#include <meas.h>
#include <page.h>
#include <storage.h>
#include <logger.h>
#include <utils.h>
#include <exception.h>

#include <iterator>
#include <list>
using namespace mdb;

BOOST_AUTO_TEST_CASE(PageCreateOpen) {
  {
    Page::Page_ptr created =
        Page::Create(mdb_test::test_page_name, mdb_test::sizeInMb10);
    BOOST_CHECK(!created->isFull());
    created->close();
  }
 {
    Page::Page_ptr openned = Page::Open(mdb_test::test_page_name);

    BOOST_CHECK_EQUAL(openned->size(), mdb_test::sizeInMb10);
    BOOST_CHECK(!openned->isFull());
    openned->close();
  }
}

BOOST_AUTO_TEST_CASE(PageOpenTwice) {
  
	Page::Page_ptr created =  Page::Create(mdb_test::test_page_name, mdb_test::sizeInMb10);
    BOOST_CHECK(!created->isFull());
	created->close();
  
  {
    Page::Page_ptr openned = Page::Open(mdb_test::test_page_name);

    BOOST_CHECK_THROW(Page::Open(mdb_test::test_page_name), utils::Exception);

    openned->close();
  }
 
}

BOOST_AUTO_TEST_CASE(PageIO) {
  const size_t TestableMeasCount = 100;
  std::string index = "";
  {
    {
      Page::Page_ptr storage = Page::Create(mdb_test::test_page_name, mdb_test::sizeInMb10);
      storage->close();
    }
    const mdb::Flag flagValue = 1;
    const mdb::Flag srcValue = 2;

    for (size_t i = 0; i < TestableMeasCount; ++i) {
      Page::Page_ptr storage = Page::Open(mdb_test::test_page_name);
      auto newMeas = mdb::Meas::empty();
      newMeas.value = i;
      newMeas.id = i;
      newMeas.flag = flagValue;
      newMeas.source = srcValue;
      newMeas.time = i;
      storage->append(newMeas);
      storage->close();
    }

    Page::Page_ptr storage = Page::Open(mdb_test::test_page_name);

    BOOST_CHECK_EQUAL(storage->minTime(), mdb::Time(0));
    BOOST_CHECK_EQUAL(storage->maxTime(), static_cast<mdb::Time>(TestableMeasCount - 1));

    auto newMeas = mdb::Meas::empty();
    for (size_t i = 0; i < TestableMeasCount; ++i) {
      bool readState = storage->read(&newMeas, i);

      BOOST_CHECK_EQUAL(readState, true);
      BOOST_CHECK_EQUAL(newMeas.value, i);
      BOOST_CHECK_EQUAL(newMeas.id, i);
      BOOST_CHECK_EQUAL(newMeas.flag, flagValue);
      BOOST_CHECK_EQUAL(newMeas.source, srcValue);

      BOOST_CHECK_EQUAL(newMeas.time, (mdb::Time)i);
    }
    BOOST_CHECK(!storage->isFull());

    auto hdr = storage->getHeader();
    BOOST_CHECK_EQUAL(hdr.maxTime, (mdb::Time)TestableMeasCount - 1);
    BOOST_CHECK_EQUAL(hdr.minTime, mdb::Time(0));
    BOOST_CHECK_EQUAL(hdr.size, storage->size());
    index = storage->index_fileName();
    storage->close();
  }

  utils::rm(mdb_test::test_page_name);
  utils::rm(index);
}

BOOST_AUTO_TEST_CASE(Capacity) {
  const size_t pageSize =
      sizeof(mdb::Page::Header) + sizeof(mdb::Meas) * 10;
  Page::Page_ptr page = Page::Create(mdb_test::test_page_name, pageSize);
  BOOST_CHECK_EQUAL(page->capacity(), (size_t)10);

  auto newMeas = mdb::Meas::empty();
  page->append(newMeas);

  BOOST_CHECK_EQUAL(page->capacity(), (size_t)9);
  page->append(newMeas);

  BOOST_CHECK_EQUAL(page->capacity(), (size_t)8);
  auto index = page->index_fileName();
  page->close();

  utils::rm(mdb_test::test_page_name);
  utils::rm(index);
}

BOOST_AUTO_TEST_CASE(AppendMany) {
  const size_t pageSize =
      sizeof(mdb::Page::Header) + sizeof(mdb::Meas) * 10;
  Page::Page_ptr page = Page::Create(mdb_test::test_page_name, pageSize);

  size_t arr_size = 15;
  mdb::Meas::PMeas array = new mdb::Meas[arr_size];
  for (size_t i = 0; i < arr_size; ++i) {
    array[i].id = i;
    array[i].time = i;
  }
  size_t writed = page->append(array, arr_size);
  delete[] array;
  BOOST_CHECK_EQUAL(writed, (size_t)10);

  for (size_t i = 0; i < writed; ++i) {
    mdb::Meas readed;
    page->read(&readed, i);
    BOOST_CHECK_EQUAL(readed.id, i);
  }

  BOOST_CHECK_EQUAL(page->minTime(), mdb::Time(0));
  BOOST_CHECK_EQUAL(page->maxTime(), (mdb::Time)writed-1);
  auto index = page->index_fileName();
  page->close();
  utils::rm(mdb_test::test_page_name);
  utils::rm(index);
}

BOOST_AUTO_TEST_CASE(PagereadIntervalFltr) {
  const int TestableMeasCount = 1000;
  std::string index = "";
  {
    {
      Page::Page_ptr storage = Page::Create(mdb_test::test_page_name, mdb_test::sizeInMb10);
      storage->close();
    }

    Page::Page_ptr storage = Page::Open(mdb_test::test_page_name);
    for (int i = 0; i < TestableMeasCount; ++i) {

      auto newMeas = mdb::Meas::empty();
      newMeas.value = i;
      newMeas.id = i % 10;
      newMeas.flag = (mdb::Flag)(i % 5);
      newMeas.source = (mdb::Flag)(i % 5);
      newMeas.time = i;
      storage->append(newMeas);
    }

    {
      IdArray ids = {1, 2, 3};
	  Meas::MeasList readRes;
      auto reader=storage->readInterval(ids, 3, 3, 0, TestableMeasCount);
	  reader->readAll(&readRes);
      reader=nullptr;

      BOOST_CHECK(readRes.size() != 0);
      for (auto it = readRes.cbegin(); it != readRes.cend(); ++it) {

        BOOST_CHECK(it->id != 4 && it->id != 5);
        BOOST_CHECK(it->flag == 3);
        BOOST_CHECK(it->source == 3);
        BOOST_CHECK(it->time <= TestableMeasCount);
      }
    }

    {
      IdArray ids = {1, 2, 3, 4, 5};
	  Meas::MeasList readRes;
      auto reader=storage->readInterval(ids, 0, 0, 0, TestableMeasCount);
	  reader->readAll(&readRes);

      reader=nullptr;
      BOOST_CHECK(readRes.size() != 0);
      bool haveFlag = false;
      bool haveSource = false;
      for (auto it = readRes.cbegin(); it != readRes.cend(); ++it) {

        if (it->source > 3) {
          haveSource = true;
        }
        if (it->flag > 3) {
          haveFlag = true;
        }
        BOOST_CHECK(it->time <= TestableMeasCount);
      }

      BOOST_CHECK(haveSource);
      BOOST_CHECK(haveFlag);
      index = storage->index_fileName();
    }
    storage->close();
  }

  utils::rm(mdb_test::test_page_name);
  utils::rm(index);
}

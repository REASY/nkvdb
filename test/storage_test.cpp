#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Main
#include <boost/test/unit_test.hpp>
#include "test_common.h"
#include <storage/Meas.h>
#include <storage/Page.h>
#include <utils/ProcessLogger.h>

using namespace storage;

BOOST_AUTO_TEST_CASE(MeasEmpty) {
	storage::Meas::PMeas pm = storage::Meas::empty();

	BOOST_CHECK_EQUAL(pm->value, storage::Value(0));
	BOOST_CHECK_EQUAL(pm->flag, storage::Flag(0));
	BOOST_CHECK_EQUAL(pm->id,   storage::Id(0));
	BOOST_CHECK_EQUAL(pm->source, storage::Flag(0));
	BOOST_CHECK_EQUAL(pm->time,   storage::Time(0));
}

BOOST_AUTO_TEST_CASE(CreateOpenStoragePage) {
	{
		Page::PPage created = Page::Create("test_page.db", mdb_test::sizeInMb10);
	}
	{
		Page::PPage openned = Page::Open("test_page.db");
		
		BOOST_CHECK_EQUAL(openned->sizeMb(), mdb_test::sizeInMb10);
	}
}

BOOST_AUTO_TEST_CASE(IOTest) {
    const int TestableMeasCount=100000;
    {
        logger<<"measCount="<<TestableMeasCount<<endl;
	Page::PPage storage = Page::Create("test_page.db", mdb_test::sizeInMb10);

        const int flagValue=1;
        const int srcValue=2;
        const int timeValue=3;

        for(int i=0;i<TestableMeasCount;++i){
            auto newMeas=storage::Meas::empty();
            newMeas->value=i;
            newMeas->id=i;
            newMeas->flag=flagValue;
            newMeas->source=srcValue;
            newMeas->time=timeValue;
            storage->write(newMeas);
        }

        for(int i=0;i<TestableMeasCount;++i){
            auto newMeas=storage::Meas::empty();
            
            bool readState=storage->read(newMeas,i);
            
            BOOST_CHECK_EQUAL(readState, true);
            BOOST_CHECK_EQUAL(newMeas->value, i);
            BOOST_CHECK_EQUAL(newMeas->id, i);
            BOOST_CHECK_EQUAL(newMeas->flag, flagValue);
            BOOST_CHECK_EQUAL(newMeas->source, srcValue);
            BOOST_CHECK_EQUAL(newMeas->time, timeValue);
        }
    }
}
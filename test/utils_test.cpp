#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Main
#include <boost/test/unit_test.hpp>

#include <utils/utils.h>


BOOST_AUTO_TEST_CASE(UtilsEmpty) {
	BOOST_CHECK_EQUAL(true, true);
}
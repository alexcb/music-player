#include <stdio.h>
#include "testRunner.h"

#include "tests.h"

#include "log.h"

unsigned int testManyFunction(void) {
    TEST_ASSERT( !testPlayerLoop() );
    return 0;
}

int main() {
	set_log_level( LOG_LEVEL_DEBUG );
    return (int) testRunner(testManyFunction);
}


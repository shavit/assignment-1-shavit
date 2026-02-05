#include "unity.h"
#include <stdbool.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include "../../examples/autotest-validate/autotest-validate.h"
#include "../../assignment-autotest/test/assignment1/username-from-conf-file.h"

/**
* This function should:
*   1) Call the my_username() function in Test_assignment_validate.c to get your hard coded username.
*   2) Obtain the value returned from function malloc_username_from_conf_file() in username-from-conf-file.h within
*       the assignment autotest submodule at assignment-autotest/test/assignment1/
*   3) Use unity assertion TEST_ASSERT_EQUAL_STRING_MESSAGE the two strings are equal.  See
*       the [unity assertion reference](https://github.com/ThrowTheSwitch/Unity/blob/master/docs/UnityAssertionsReference.md)
*/
void test_validate_my_username()
{
    const char* expect_u = my_username();
    int fd = open("./conf/username.txt", O_RDONLY);
    if (fd == -1) exit(-1);
    char buf[7];
    read(fd, buf, 6);
    buf[6] = '\0';
    close(fd);

    TEST_ASSERT_TRUE_MESSAGE(0 == strncmp(buf, expect_u, 7),"Unmatched usernames");
}

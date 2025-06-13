#include <stdlib.h>
#include <check.h>
#include <string.h>

#include "../roman.h"

START_TEST(test_roman_zero)
{
    const char *result = roman(0);
    ck_assert_str_eq(result, "");
}
END_TEST

START_TEST(test_roman_basic_numbers)
{
    ck_assert_str_eq(roman(1), "I");
    ck_assert_str_eq(roman(5), "V");
    ck_assert_str_eq(roman(10), "X");
    ck_assert_str_eq(roman(50), "L");
    ck_assert_str_eq(roman(100), "C");
    ck_assert_str_eq(roman(500), "D");
    ck_assert_str_eq(roman(1000), "M");
}
END_TEST

START_TEST(test_roman_combined_numbers)
{
    ck_assert_str_eq(roman(2), "II");
    ck_assert_str_eq(roman(3), "III");
    ck_assert_str_eq(roman(6), "VI");
    ck_assert_str_eq(roman(7), "VII");
    ck_assert_str_eq(roman(11), "XI");
    ck_assert_str_eq(roman(15), "XV");
    ck_assert_str_eq(roman(16), "XVI");
}
END_TEST

START_TEST(test_roman_subtractive_notation)
{
    ck_assert_str_eq(roman(4), "IV");
    ck_assert_str_eq(roman(9), "IX");
    ck_assert_str_eq(roman(40), "XL");
    ck_assert_str_eq(roman(90), "XC");
    ck_assert_str_eq(roman(400), "CD");
    ck_assert_str_eq(roman(900), "CM");
}
END_TEST

START_TEST(test_roman_complex_numbers)
{
    ck_assert_str_eq(roman(14), "XIV");
    ck_assert_str_eq(roman(19), "XIX");
    ck_assert_str_eq(roman(24), "XXIV");
    ck_assert_str_eq(roman(42), "XLII");
    ck_assert_str_eq(roman(99), "XCIX");
    ck_assert_str_eq(roman(490), "CDXC");
    ck_assert_str_eq(roman(999), "CMXCIX");
}
END_TEST

START_TEST(test_roman_thousands)
{
    ck_assert_str_eq(roman(1001), "MI");
    ck_assert_str_eq(roman(1984), "MCMLXXXIV");
    ck_assert_str_eq(roman(2023), "MMXXIII");
    ck_assert_str_eq(roman(3999), "MMMCMXCIX");
}
END_TEST

START_TEST(test_roman_special_cases)
{
    // The famous year from the function comment
    ck_assert_str_eq(roman(1999), "MCMXCIX");

    // Check boundary values
    ck_assert_str_eq(roman(1), "I");           // Min value that produces output
    ck_assert_str_eq(roman(4999), "MMMMCMXCIX"); // Large value
}
END_TEST

Suite* roman_suite(void)
{
    Suite *s = suite_create("Roman");
    TCase *tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_roman_zero);
    tcase_add_test(tc_core, test_roman_basic_numbers);
    tcase_add_test(tc_core, test_roman_combined_numbers);
    tcase_add_test(tc_core, test_roman_subtractive_notation);
    tcase_add_test(tc_core, test_roman_complex_numbers);
    tcase_add_test(tc_core, test_roman_thousands);
    tcase_add_test(tc_core, test_roman_special_cases);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = roman_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

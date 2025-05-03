#include <unity.h>
#include "test_inc/adc_test.h"


void test_adc_to_celsius(void){
    // Note-se que o último argumento é trabalhado como int.
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 27.0f , adc_to_celsius(876)); 
}

void setUp(){};
void tearDown(){};

int main(void){
    UNITY_BEGIN();

    RUN_TEST(test_adc_to_celsius);

    return UNITY_END();
}
#include <string.h>

#include "roman.h"

/*******************************************************************\
 *                                                                 *
 * Conceived on VII June MCMXCIX ( or should that be MIM ? )       *
 *                                                                 *
 *******************************************************************/
const char *roman( unsigned n )
{
    static const char* rom[3][10] = {
        {"","C","CC","CCC","CD","D","DC","DCC","DCCC","CM"},
        {"","X","XX","XXX","XL","L","LX","LXX","LXXX","XC"},
        {"","I","II","III","IV","V","VI","VII","VIII","IX"}
    };

    int mille = n / 1000;
    int rest  = n % 1000;

    static char ret[1024];
    ret[0] = '\0';  /* Initialize buffer to empty string */
    char *q = ret;

    for( int i = 0; i < mille && q < ret + sizeof(ret) - 1; i++ )
    {
        *q++ = 'M';
    }
    *q = '\0';  /* Properly null-terminate */
    mille = rest / 100;
    rest %= 100;
    strncat( ret, rom[0][mille], sizeof(ret) - strlen(ret) - 1 );

    mille = rest / 10;
    rest %= 10;
    strncat( ret, rom[1][mille], sizeof(ret) - strlen(ret) - 1 );
    strncat( ret, rom[2][rest], sizeof(ret) - strlen(ret) - 1 );

    return ret;
}

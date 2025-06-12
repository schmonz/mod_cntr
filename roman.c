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
    char *q;

    for( q=ret; mille--; )
    {
        *q++ = 'M';
    }
    mille = rest / 100;
    rest %= 100;
    strcat( ret, rom[0][mille] );

    mille = rest / 10;
    rest %= 10;
    strcat( ret, rom[1][mille] );
    strcat( ret, rom[2][rest] );

    return ret;
}

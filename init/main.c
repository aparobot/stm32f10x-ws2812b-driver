#include "ws2812b.h"

int main()
{
    ws2812bInit();
    
    if(!ws2812bTest()) while(1);

    ws2812bSet(0, 0xff0000);           //green
    for(int i = 5000000; i > 0; i--);
    ws2812bSet(1, 0x00ff00);           //red
    for(int i = 5000000; i > 0; i--);
    ws2812bSet(2, 0x0000ff);           //blue
    for(int i = 5000000; i > 0; i--);
    ws2812bClearAll();
    for(int i = 5000000; i > 0; i--);
    while(1)
    {
        ws2812bSet(0, 0xff0000);
        for(int i = 1000000; i > 0; i--);
        ws2812bClear(0);
        for(int i = 1000000; i > 0; i--);
    }

}

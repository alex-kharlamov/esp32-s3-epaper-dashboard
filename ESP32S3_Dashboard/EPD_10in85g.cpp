/*****************************************************************************
* | File      	:  	EPD_10in85g.c
* | Author      :   Waveshare team
* | Function    :   10.85inch e-paper (G)
* | Info        :
*----------------
* |	This version:   V1.0
* | Date        :   2024-08-19
* | Info        :
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
******************************************************************************/
#include "EPD_10in85g.h"
#include "Debug.h"

/******************************************************************************
function :	Software reset
parameter:
******************************************************************************/
static void EPD_10in85g_Reset(void)
{
    DEV_Digital_Write(EPD_RST_PIN, 1);
    DEV_Delay_ms(20);
    DEV_Digital_Write(EPD_RST_PIN, 0);
    DEV_Delay_ms(10);
    DEV_Digital_Write(EPD_RST_PIN, 1);
    DEV_Delay_ms(20);
}

/******************************************************************************
function :	send command
parameter:
     Reg : Command register
******************************************************************************/
static void EPD_10in85g_SendCommand_0(UBYTE Reg)
{
    DEV_Digital_Write(EPD_DC_PIN, 0);
    DEV_Digital_Write(EPD_CS_M_PIN, 0);
    DEV_SPI_WriteByte(Reg);
    DEV_Digital_Write(EPD_CS_M_PIN, 1);
}
static void EPD_10in85g_SendCommand_1(UBYTE Reg)
{
    DEV_Digital_Write(EPD_DC_PIN, 0);
    DEV_Digital_Write(EPD_CS_S_PIN, 0);
    DEV_SPI_WriteByte(Reg);
    DEV_Digital_Write(EPD_CS_S_PIN, 1);
}
static void EPD_10in85g_SendCommand_ALL(UBYTE Reg)
{
    DEV_Digital_Write(EPD_DC_PIN, 0);
    DEV_Digital_Write(EPD_CS_M_PIN, 0);
    DEV_Digital_Write(EPD_CS_S_PIN, 0);
    DEV_SPI_WriteByte(Reg);
    DEV_Digital_Write(EPD_CS_M_PIN, 1);
    DEV_Digital_Write(EPD_CS_S_PIN, 1);
}

/******************************************************************************
function :	send data
parameter:
    Data : Write data
******************************************************************************/

static void EPD_10in85g_SendData_0(UBYTE Data)
{
    DEV_Digital_Write(EPD_DC_PIN, 1);
    DEV_Digital_Write(EPD_CS_M_PIN, 0);
    DEV_SPI_WriteByte(Data);
    DEV_Digital_Write(EPD_CS_M_PIN, 1);
}
static void EPD_10in85g_SendData_1(UBYTE Data)
{
    DEV_Digital_Write(EPD_DC_PIN, 1);
    DEV_Digital_Write(EPD_CS_S_PIN, 0);
    DEV_SPI_WriteByte(Data);
    DEV_Digital_Write(EPD_CS_S_PIN, 1);
}

static void EPD_10in85g_SendnData_0(UBYTE *Data, UDOUBLE Len)
{
    DEV_Digital_Write(EPD_DC_PIN, 1);
    DEV_Digital_Write(EPD_CS_M_PIN, 0);
    DEV_SPI_Write_nByte(Data, Len);
    DEV_Digital_Write(EPD_CS_M_PIN, 1);
}
static void EPD_10in85g_SendnData_1(UBYTE *Data, UDOUBLE Len)
{
    DEV_Digital_Write(EPD_DC_PIN, 1);
    DEV_Digital_Write(EPD_CS_S_PIN, 0);
    DEV_SPI_Write_nByte(Data, Len);
    DEV_Digital_Write(EPD_CS_S_PIN, 1);
}

static void EPD_10in85g_SendData_ALL(UBYTE Data)
{
    DEV_Digital_Write(EPD_DC_PIN, 1);
    DEV_Digital_Write(EPD_CS_M_PIN, 0);
    DEV_Digital_Write(EPD_CS_S_PIN, 0);
    DEV_SPI_WriteByte(Data);
    DEV_Digital_Write(EPD_CS_M_PIN, 1);
    DEV_Digital_Write(EPD_CS_S_PIN, 1);
}

/******************************************************************************
function :	Wait until the busy_pin goes LOW
parameter:
******************************************************************************/
void EPD_10in85g_ReadBusy(void)
{
    uint32_t start = millis();
    Serial.printf("BUSY wait: %s, level=%d (LOW=busy)\n", demoStage, digitalRead(EPD_BUSY_PIN));
    while (digitalRead(EPD_BUSY_PIN) == LOW) {
        if (millis() - start >= 90000) demoAbort("BUSY timeout (90 seconds)");
        delay(10);
    }
    delay(10);
    Serial.printf("BUSY released: %s after %lu ms\n", demoStage, (unsigned long)(millis()-start));
}

/******************************************************************************
function :	Turn On Display
parameter:
******************************************************************************/
static void EPD_10in85g_TurnOnDisplay(void)
{

	demoStage = "refresh";
    Serial.println("Refresh start");
    uint32_t refreshStart = millis();
	EPD_10in85g_SendCommand_ALL(0x12);
    EPD_10in85g_SendData_ALL(0x00);
    // A missing/disconnected BUSY line must not produce a false success.
    while (digitalRead(EPD_BUSY_PIN) == HIGH) {
        if (millis() - refreshStart > 1000) demoAbort("BUSY never asserted after refresh");
        delay(1);
    }
    EPD_10in85g_ReadBusy();
    Serial.printf("Refresh complete after %lu ms (image needs visual confirmation)\n", (unsigned long)(millis()-refreshStart));
}

/******************************************************************************
function :	Initialize the e-Paper register
parameter:
******************************************************************************/
void EPD_10in85g_Init(void)
{
	demoStage = "reset";
	EPD_10in85g_Reset();
    EPD_10in85g_ReadBusy();

    EPD_10in85g_SendCommand_ALL(0x4D);
    EPD_10in85g_SendData_ALL(0x78);	

    EPD_10in85g_SendCommand_ALL(0xE0);
    EPD_10in85g_SendData_ALL(0x01);

    EPD_10in85g_SendCommand_ALL(0xE5);
    EPD_10in85g_SendData_ALL(0x08); 

    EPD_10in85g_SendCommand_ALL(0xA2);
    EPD_10in85g_SendData_ALL(0x01); 

    EPD_10in85g_SendCommand_ALL(0x00);	//0x00
    EPD_10in85g_SendData_ALL(0x2F);	
    EPD_10in85g_SendData_ALL(0x21);	

    EPD_10in85g_SendCommand_ALL(0xA2);
    EPD_10in85g_SendData_ALL(0x02); 

    EPD_10in85g_SendCommand_ALL(0x00);	//0x00
    EPD_10in85g_SendData_ALL(0x2F);	
    EPD_10in85g_SendData_ALL(0x21);	

    EPD_10in85g_SendCommand_ALL(0xA2);
    EPD_10in85g_SendData_ALL(0x00); 

    EPD_10in85g_SendCommand_ALL(0x01);	//0x00
    EPD_10in85g_SendData_ALL(0x07);	
    EPD_10in85g_SendData_ALL(0x00);	

    EPD_10in85g_SendCommand_ALL(0x06);//47uH
    EPD_10in85g_SendData_ALL(0x0d);
    EPD_10in85g_SendData_ALL(0x12);
    EPD_10in85g_SendData_ALL(0x30);
    EPD_10in85g_SendData_ALL(0x20);
    EPD_10in85g_SendData_ALL(0x19);
    EPD_10in85g_SendData_ALL(0x3D);
    EPD_10in85g_SendData_ALL(0x0C);

    EPD_10in85g_SendCommand_ALL(0x30);
    EPD_10in85g_SendData_ALL(0x08); 

    EPD_10in85g_SendCommand_ALL(0x50);	//0x50
    EPD_10in85g_SendData_ALL(0x37);	

    EPD_10in85g_SendCommand_ALL(0x61);//0x61	
    EPD_10in85g_SendData_ALL(EPD_10in85g_WIDTH/256);	
    EPD_10in85g_SendData_ALL(EPD_10in85g_WIDTH%256);	
    EPD_10in85g_SendData_ALL(EPD_10in85g_HEIGHT/256);	
    EPD_10in85g_SendData_ALL(EPD_10in85g_HEIGHT%256);	

    EPD_10in85g_SendCommand_ALL(0x65);	//0x65
    EPD_10in85g_SendData_ALL(0x00);
    EPD_10in85g_SendData_ALL(0x00);
    EPD_10in85g_SendData_ALL(0x00);
    EPD_10in85g_SendData_ALL(0x00);

    EPD_10in85g_SendCommand_ALL(0xE3);
    EPD_10in85g_SendData_ALL(0x88);  

    EPD_10in85g_SendCommand_ALL(0xE9);
    EPD_10in85g_SendData_ALL(0x01);   

    EPD_10in85g_SendCommand_ALL(0xB8);
    EPD_10in85g_SendData_ALL(0xB5);  
    DEV_Delay_ms(200);

    demoStage = "power on";
    EPD_10in85g_SendCommand_ALL(0x04); //Power on
    DEV_Delay_ms(500);
    EPD_10in85g_ReadBusy();          //waiting for the electronic paper IC to release the idle signal
}

void EPD_10in85g_Init_Fast(void)
{
	demoStage = "reset";
	EPD_10in85g_Reset();
    EPD_10in85g_ReadBusy();

    EPD_10in85g_SendCommand_ALL(0x4D);
    EPD_10in85g_SendData_ALL(0x78);	

    EPD_10in85g_SendCommand_ALL(0xE0);
    EPD_10in85g_SendData_ALL(0x01);

    EPD_10in85g_SendCommand_ALL(0xE5);
    EPD_10in85g_SendData_ALL(0x08); 

    EPD_10in85g_SendCommand_ALL(0xA2);
    EPD_10in85g_SendData_ALL(0x01); 

    EPD_10in85g_SendCommand_ALL(0x00);	//0x00
    EPD_10in85g_SendData_ALL(0x2F);	
    EPD_10in85g_SendData_ALL(0x21);	

    EPD_10in85g_SendCommand_ALL(0xA2);
    EPD_10in85g_SendData_ALL(0x02); 

    EPD_10in85g_SendCommand_ALL(0x00);	//0x00
    EPD_10in85g_SendData_ALL(0x2F);	
    EPD_10in85g_SendData_ALL(0x21);	

    EPD_10in85g_SendCommand_ALL(0xA2);
    EPD_10in85g_SendData_ALL(0x00); 

    EPD_10in85g_SendCommand_ALL(0x01);	//0x00
    EPD_10in85g_SendData_ALL(0x07);	
    EPD_10in85g_SendData_ALL(0x00);	

    EPD_10in85g_SendCommand_ALL(0x06);//47uH
    EPD_10in85g_SendData_ALL(0x0d);
    EPD_10in85g_SendData_ALL(0x12);
    EPD_10in85g_SendData_ALL(0x30);
    EPD_10in85g_SendData_ALL(0x20);
    EPD_10in85g_SendData_ALL(0x19);
    EPD_10in85g_SendData_ALL(0x3D);
    EPD_10in85g_SendData_ALL(0x0C);

    EPD_10in85g_SendCommand_ALL(0x30);
    EPD_10in85g_SendData_ALL(0x08); 

    EPD_10in85g_SendCommand_ALL(0x50);	//0x50
    EPD_10in85g_SendData_ALL(0x37);	

    EPD_10in85g_SendCommand_ALL(0x61);//0x61	
    EPD_10in85g_SendData_ALL(EPD_10in85g_WIDTH/256);	
    EPD_10in85g_SendData_ALL(EPD_10in85g_WIDTH%256);	
    EPD_10in85g_SendData_ALL(EPD_10in85g_HEIGHT/256);	
    EPD_10in85g_SendData_ALL(EPD_10in85g_HEIGHT%256);	

    EPD_10in85g_SendCommand_ALL(0x65);	//0x65
    EPD_10in85g_SendData_ALL(0x00);	
    EPD_10in85g_SendData_ALL(0x00);	
    EPD_10in85g_SendData_ALL(0x00);	
    EPD_10in85g_SendData_ALL(0x00);	

    EPD_10in85g_SendCommand_ALL(0xE3);
    EPD_10in85g_SendData_ALL(0x88);  

    EPD_10in85g_SendCommand_ALL(0xE9);
    EPD_10in85g_SendData_ALL(0x01);   

    EPD_10in85g_SendCommand_ALL(0xB8);
    EPD_10in85g_SendData_ALL(0xB5);  
    DEV_Delay_ms(200);

    demoStage = "power on";
    EPD_10in85g_SendCommand_ALL(0x04); //Power on
    DEV_Delay_ms(500);
    EPD_10in85g_ReadBusy();          //waiting for the electronic paper IC to release the idle signal

    //Fast
	EPD_10in85g_SendCommand_ALL(0xE0);
	EPD_10in85g_SendData_ALL(0x03);    			
	EPD_10in85g_SendCommand_ALL(0xE6);
	EPD_10in85g_SendData_ALL(92);
	EPD_10in85g_SendCommand_ALL(0xA5);		
	EPD_10in85g_SendData_ALL(0x00);
	EPD_10in85g_ReadBusy();          //waiting for the electronic paper IC to release the idle signal
}


/******************************************************************************
function :	Clear screen
parameter:
******************************************************************************/
void EPD_10in85g_Clear(UBYTE color)
{
	UWORD Width, Height;
    Width = (EPD_10in85g_WIDTH % 4 == 0)? (EPD_10in85g_WIDTH / 4 ): (EPD_10in85g_WIDTH / 4 + 1);
    Height = EPD_10in85g_HEIGHT;
    UBYTE Color = (color << 6) | (color << 4) | (color << 2) | color;

    Serial.println("Write CS_M / left controller");
    EPD_10in85g_SendCommand_0(0x10);
    for (UWORD j = 0; j < Height; j++) {
        if ((j % 16) == 0) delay(1);
        for (UWORD i = 0; i < Width; i++) {
            EPD_10in85g_SendData_0(Color);
        }
    }	

    Serial.println("Write CS_S / right controller");
    EPD_10in85g_SendCommand_1(0x10);
    for (UWORD j = 0; j < Height; j++) {
        if ((j % 16) == 0) delay(1);
        for (UWORD i = 0; i < Width; i++) {
            EPD_10in85g_SendData_1(Color);
        }
    }	

	EPD_10in85g_TurnOnDisplay();
}

/******************************************************************************
function :	Sends the image buffer in RAM to e-Paper and displays
parameter:
	Image : Image data
******************************************************************************/
static void EPD_10in85g_WriteFrame(const UBYTE *Image)
{
	UWORD Width, Height;
    Width = (EPD_10in85g_WIDTH % 4 == 0)? (EPD_10in85g_WIDTH / 4 ): (EPD_10in85g_WIDTH / 4 + 1);
    Height = EPD_10in85g_HEIGHT;
	
    Serial.println("Write CS_M / left controller");
    EPD_10in85g_SendCommand_0(0x10);
    for (UWORD j = 0; j < Height; j++) {
        if ((j % 16) == 0) delay(1);
        for (UWORD i = 0; i < Width; i++) {
            EPD_10in85g_SendData_0(Image[j*2*Width + i]);
        }
    }	

    Serial.println("Write CS_S / right controller");
    EPD_10in85g_SendCommand_1(0x10);
    for (UWORD j = 0; j < Height; j++) {
        if ((j % 16) == 0) delay(1);
        for (UWORD i = 0; i < Width; i++) {
            EPD_10in85g_SendData_1(Image[j*2*Width + i + Width]);
        }
    }

}

// JD79665AA R83h, manufacturer manual page 40. This selects the source-output
// window for the refresh, after the complete SRAM image has been transferred.
// Four-pixel horizontal alignment is imposed by the controller. Gate scanning
// continues outside the window, so panel-level visual preservation must be tested.
static void EPD_10in85g_Window(int controller, UWORD x0, UWORD y0, UWORD x1, UWORD y1)
{
    UBYTE params[]={UBYTE((x0>>8)&3),UBYTE(x0&0xFC),
                    UBYTE((x1>>8)&3),UBYTE(x1&0xFC),
                    UBYTE((y0>>8)&3),UBYTE(y0&255),
                    UBYTE((y1>>8)&3),UBYTE(y1&255),0x01};
    // PTH_EN=0: source output follows horizontal start/end. PMODE=1.
    if(controller==0)EPD_10in85g_SendCommand_0(0x83);else EPD_10in85g_SendCommand_1(0x83);
    for(UBYTE value:params) {
        if(controller==0)EPD_10in85g_SendData_0(value);else EPD_10in85g_SendData_1(value);
    }
    Serial.printf("PARTIAL_WINDOW: controller=%d x=%u..%u y=%u..%u\n",controller,x0,x1,y0,y1);
}
void EPD_10in85g_Display(const UBYTE *Image)
{
    EPD_10in85g_WriteFrame(Image);
    EPD_10in85g_TurnOnDisplay();
}
void EPD_10in85g_DisplayWindow(const UBYTE *Image, UWORD x, UWORD y, UWORD width, UWORD height)
{
    const unsigned endX=unsigned(x)+width-1,endY=unsigned(y)+height-1;
    // This dashboard clock crosses both 680-pixel controllers. Reject unsupported
    // geometry instead of accidentally refreshing an unmasked controller.
    if(!width || !height || x>=680 || endX<680 || endX>=1360 || endY>=480 || (x%4) || (width%4))
        demoAbort("Invalid cross-controller partial-window geometry");
    EPD_10in85g_WriteFrame(Image);
    EPD_10in85g_Window(0,x,y,679,endY);
    EPD_10in85g_Window(1,0,y,endX-680,endY);
    EPD_10in85g_TurnOnDisplay();
}

void EPD_10in85g_Display_2(const UBYTE *Image)
{
	UWORD Width, Height;
    Width = (EPD_10in85g_WIDTH % 4 == 0)? (EPD_10in85g_WIDTH / 4 ): (EPD_10in85g_WIDTH / 4 + 1);
    Height = EPD_10in85g_HEIGHT/4;
	
    Serial.println("Write CS_M / left controller");
    EPD_10in85g_SendCommand_0(0x10);
    for (UWORD j = 0; j < Height; j++) {
        if ((j % 16) == 0) delay(1);
        for (UWORD i = 0; i < Width; i++) {
            EPD_10in85g_SendData_0(Image[j*Width + i]);
        }
    }	
    for (UWORD j = 0; j < Height*3; j++) {
        for (UWORD i = 0; i < Width; i++) {
            EPD_10in85g_SendData_0(0x55);
        }
    }

    Serial.println("Write CS_S / right controller");
    EPD_10in85g_SendCommand_1(0x10);
    for (UWORD j = 0; j < Height; j++) {
        if ((j % 16) == 0) delay(1);
        for (UWORD i = 0; i < Width; i++) {
            EPD_10in85g_SendData_1(Image[j*Width + i]);
        }
    }
    for (UWORD j = 0; j < Height*3; j++) {
        for (UWORD i = 0; i < Width; i++) {
            EPD_10in85g_SendData_1(0x55);
        }
    }

	EPD_10in85g_TurnOnDisplay();	
}

void EPD_10in85g_DisplayPart(const UBYTE *Image, UWORD xstart, UWORD ystart, UWORD image_width, UWORD image_heigh)
{
    UDOUBLE Width, Height;
    Width = (EPD_10in85g_WIDTH % 4 == 0)? (EPD_10in85g_WIDTH / 4 ): (EPD_10in85g_WIDTH / 4 + 1);
    Height = EPD_10in85g_HEIGHT;
    
    UWORD Xend = ((xstart + image_width)%4 == 0)?((xstart + image_width) / 4 - 1): ((xstart + image_width) / 4 );
    UWORD Yend = ystart + image_heigh-1;
    xstart = xstart / 4;
    
    if(xstart > 170 )
    {
        Xend = Xend - 170;
        xstart = xstart - 170;
        Serial.println("Write CS_M / left controller");
    EPD_10in85g_SendCommand_0(0x10);
        for (UDOUBLE i = 0; i < Height; i++) {
            for (UDOUBLE j = 0; j < Width; j++) {
                EPD_10in85g_SendData_0(0x55);
            }
        }

        Serial.println("Write CS_S / right controller");
    EPD_10in85g_SendCommand_1(0x10);
        for (UDOUBLE i = 0; i < Height; i++) {
            for (UDOUBLE j = 0; j < Width; j++) {
                if((i<Yend) && (i>=ystart) && (j<Xend) && (j>=xstart)) {
                    EPD_10in85g_SendData_1(Image[(j-xstart) + (image_width/4*(i-ystart))]);
                }
                else
                    EPD_10in85g_SendData_1(0x55);
            }
        }

    }
    else if(Xend < 170 )
    {
        Serial.println("Write CS_M / left controller");
    EPD_10in85g_SendCommand_0(0x10);
        for (UDOUBLE i = 0; i < Height; i++) {
            for (UDOUBLE j = 0; j < Width; j++) {
                if((i<Yend) && (i>=ystart) && (j<Xend) && (j>=xstart)) {
                    EPD_10in85g_SendData_0(Image[(j-xstart) + (image_width/4*(i-ystart))]);
                }
                else
                    EPD_10in85g_SendData_0(0x55);
            }
        }
        
        Serial.println("Write CS_S / right controller");
    EPD_10in85g_SendCommand_1(0x10);
        for (UDOUBLE i = 0; i < Height; i++) {
            for (UDOUBLE j = 0; j < Width; j++) {
                EPD_10in85g_SendData_1(0x55);
            }
        }
    }
    else
    {
        Serial.println("Write CS_M / left controller");
    EPD_10in85g_SendCommand_0(0x10);
        for (UDOUBLE i = 0; i < Height; i++) {
            for (UDOUBLE j = 0; j < Width; j++) {
                if((i<Yend) && (i>=ystart) && (j>=xstart)) {
                    EPD_10in85g_SendData_0(Image[(j-xstart) + (image_width/4*(i-ystart))]);
                }
                else
                    EPD_10in85g_SendData_0(0x55);
            }
        }

        Serial.println("Write CS_S / right controller");
    EPD_10in85g_SendCommand_1(0x10);
        for (UDOUBLE i = 0; i < Height; i++) {
            for (UDOUBLE j = 0; j < Width; j++) {
                if((i<Yend) && (i>=ystart) && (j<Xend-170)) {
                    EPD_10in85g_SendData_1(Image[(j+170-xstart) + (image_width/4*(i-ystart))]);
                }
                else
                    EPD_10in85g_SendData_1(0x55);
            }
        }
    }

    EPD_10in85g_TurnOnDisplay();
}

/******************************************************************************
function :	Enter sleep mode
parameter:
******************************************************************************/
void EPD_10in85g_Sleep(void)
{
    demoStage = "sleep power off";
    EPD_10in85g_SendCommand_ALL(0x02); 
    EPD_10in85g_SendData_ALL(0x00); 
	DEV_Delay_ms(100);  
	EPD_10in85g_ReadBusy();       

	EPD_10in85g_SendCommand_ALL(0X07); //enter deep sleep
	EPD_10in85g_SendData_ALL(0xA5); 
	DEV_Delay_ms(100);
}

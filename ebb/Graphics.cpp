
#include "mbed.h"
#include "Graphics.h"


Graphics::Graphics(uint16_t *pFrmBuf, uint16_t dispWidth, uint16_t dispHeight)
{
	this->windowX = dispWidth;
	this->windowY = dispHeight;
	this->pFrmBuf = pFrmBuf;
}

void Graphics::setFrameBuffer( uint16_t *pFrmBuf )
{
	this->pFrmBuf = pFrmBuf;
}

int32_t Graphics::abs(int32_t v1) const
{
   if (v1 > 0)
     return v1;

  return -v1;
}

/***********************************************************************
 *
 * Function: swim_put_line_raw
 *
 * Purpose: Draw a line on the physical display
 *
 * Processing:
 *     See function.
 *
 * Parameters:
 *     win : Window identifier
 *     x1  : Physical X position of X line start
 *     y1  : Physical Y position of Y line start
 *     x2  : Physical X position of X line end
 *     y2  : Physical Y position of Y line end
 *
 * Outputs: None
 *
 * Returns: Nothing
 *
 * Notes: None
 *
 **********************************************************************/
void Graphics::put_line(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int16_t color)
{
    int32_t e2, sx, sy, dx, dy, err;
    
    /* calculate delta_x and delta_y */
    dx = abs(x2 - x1);
    dy = abs(y2 - y1);

    /* set the direction for the step for both x and y, and
       initialize the error */
    if (x1 < x2)
        sx = 1;
    else
        sx = -1;

    if (y1 < y2)
        sy = 1;
    else
        sy = -1;

    err = dx - dy;
   
    while (1)
    {
        if ((x1 >= 0) && (x1 < this->windowX) && 
            (y1 >= 0) && (y1 < this->windowY))
            this->pFrmBuf[x1 + (this->windowX*y1)] = color;

        if ((x1 == x2) && (y1 == y2))
            return;

        e2 = 2 * err;
        if (e2 > -dy)
        {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx)
        {
            err += dx;
            y1 += sy;
        }
    }
}

/***********************************************************************
 *
 * Function: plot4points
 *
 * Purpose:
 *
 * Processing:
 *     See function.
 *
 * Parameters:
 *     win    : Window identifier
 *     cx     :
 *     cy     :
 *     x      :
 *     y      :
 *     Filled :
 *
 * Outputs: None
 *
 * Returns: Nothing
 *
 * Notes: None
 *
 **********************************************************************/
void Graphics::plot4points( int32_t cx, int32_t cy, int32_t x, int32_t y, int16_t color, int32_t Filled )
   {
   int16_t x0, x1, y0, y1;

   y0 = cy + y;
   y1 = cy - y;
   if( Filled )
      {
      for( x0=cx - x; x0<=cx + x; x0++ )
         {
         if ((x0>=0) && (x0<this->windowX) && (y0>=0) && (y0<this->windowY))
            this->pFrmBuf[x0 + (this->windowX*y0)] = color;
         if ((x0>=0) && (x0<this->windowX) && (y1>=0) && (y1<this->windowY))
            this->pFrmBuf[x0 + (this->windowX*y1)] = color;
         }
      }
   else
      {
      x0 = cx + x;
      x1 = cx - x;
      if ((x0>=0) && (x0<this->windowX) && (y0>=0) && (y0<this->windowY))
         this->pFrmBuf[x0 + (this->windowX*y0)] = color;
      if ((x != 0) && 
          (x1>=0) && (x1<this->windowX) && (y0>=0) && (y0<this->windowY))
         this->pFrmBuf[x1 + (this->windowX*y0)] = color;
      if ((y != 0) &&
          (x0>=0) && (x0<this->windowX) && (y1>=0) && (y1<this->windowY))
         this->pFrmBuf[x0 + (this->windowX*y1)] = color;
      if ((x != 0 && y != 0) &&
          (x1>=0) && (x1<this->windowX) && (y1>=0) && (y1<this->windowY))
         this->pFrmBuf[x1 + (this->windowX*y1)] = color;
      }
   }

/***********************************************************************
 *
 * Function: plot8points
 *
 * Purpose:
 *
 * Processing:
 *     See function.
 *
 * Parameters:
 *     win    : Window identifier
 *     cx     :
 *     cy     :
 *     x      :
 *     y      :
 *     Filled :
 *
 * Outputs: None
 *
 * Returns: Nothing
 *
 * Notes: None
 *
 **********************************************************************/
void Graphics::plot8points( int32_t cx, int32_t cy, int32_t x, int32_t y, int16_t color, int32_t Filled )
   {
   plot4points( cx, cy, x, y, color, Filled );
   if (x != y)
      plot4points( cx, cy, y, x, color, Filled );
   }

/***********************************************************************
 *
 * Function: swim_put_circle
 *
 * Purpose:
 *
 * Processing:
 *     See function.
 *
 * Parameters:
 *     win    : Window identifier
 *     cx     :
 *     cy     :
 *     radius :
 *     Filled :
 *
 * Outputs: None
 *
 * Returns: Nothing
 *
 * Notes: None
 *
 **********************************************************************/
void Graphics::put_circle( int32_t cx, int32_t cy, int16_t color, int32_t radius, int32_t Filled )
   {
   int32_t Error = -radius;
   int16_t x = radius;
   int16_t y = 0;

   while( x >= y )
      {
      plot8points( cx, cy, x, y, color, Filled );

      Error += y;
      ++y;
      Error += y;

      if( Error >= 0 )
         {
         --x;
         Error -= x;
         Error -= x;
         }
      }
   }

void Graphics::put_dot( int32_t cx, int32_t cy, int16_t color )
{
  int size = 1;
  for (int y1 = cy - size; y1 <= (cy + size); y1++) {
    for (int x1 = cx - size; x1 <= (cx + size); x1++) {
      if ((x1 >= 0) && (x1 < this->windowX) && (y1 >= 0) && (y1 < this->windowY)) {
          this->pFrmBuf[x1 + (this->windowX*y1)] = color;
      }
    }
  }
}




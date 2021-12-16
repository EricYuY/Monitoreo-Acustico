//Always use include guards
#ifndef MICROFONO_H
#define MIROFONO_H

#include <driver/i2s.h>
#include "sos-iir-filter.h"

 //////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////Configuration /////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////

#define LEQ_PERIOD        0.1          // second(s)
#define WEIGHTING         A_weighting // Ponderacion A y C
#define LEQ_UNITS         "LAeq"      //
#define DB_UNITS          "dBA"       //


#define MIC_EQUALIZER     INMP441    // MICROFONO selecionado
#define MIC_OFFSET_DB     -6.0103      // offset (sine-wave RMS vs. dBFS)- Calibracion si se requiere (lineal)

// Datasheet
#define MIC_SENSITIVITY   -26         // DATASHEET
#define MIC_REF_DB        94.0        // DATASHEET
#define MIC_OVERLOAD_DB   120.0       // DATASHEET
#define MIC_NOISE_DB      33          // DATASHEET
#define MIC_BITS          24          // DATASHEET
#define MIC_CONVERT(s)    (s >> (SAMPLE_BITS - MIC_BITS))
#define MIC_TIMING_SHIFT  0           //

constexpr double MIC_REF_AMPL = pow(10, double(MIC_SENSITIVITY)/20) * ((1<<(MIC_BITS-1))-1); // Referencia

//PINES definidos para I2s con tarjeta

#define I2S_WS            21
#define I2S_SCK           19 
#define I2S_SD            18 
#define I2S_PORT          I2S_NUM_0 //puerto


//////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////// IIR Filters (filtro para microfono) ///////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////


// DC-Blocker filter - removes DC component from I2S data
// See: https://www.dsprelated.com/freebooks/filters/DC_Blocker.html
// a1 = -0.9992 should heavily attenuate frequencies below 10Hz
SOS_IIR_Filter DC_BLOCKER = { 
  gain: 1.0,
  sos: {{-1.0, 0.0, +0.9992, 0}}
};

// TDK/InvenSense INMP441
// Datasheet: https://www.invensense.com/wp-content/uploads/2015/02/INMP441.pdf
// B ~= [1.00198, -1.99085, 0.98892]
// A ~= [1.0, -1.99518, 0.99518]
SOS_IIR_Filter INMP441 = {
  gain: 1.00197834654696, 
  sos: { // Second-Order Sections {b1, b2, -a1, -a2}
    {-1.986920458344451, +0.986963226946616, +1.995178510504166, -0.995184322194091}
  }
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////// Weighting filters////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////

// A-weighting IIR Filter, Fs = 48KHz 
// (Referencia Dr. Matt L., Source: https://dsp.stackexchange.com/a/36122)
// B = [0.169994948147430, 0.280415310498794, -1.120574766348363, 0.131562559965936, 0.974153561246036, -0.282740857326553, -0.152810756202003]
// A = [1.0, -2.12979364760736134, 0.42996125885751674, 1.62132698199721426, -0.96669962900852902, 0.00121015844426781, 0.04400300696788968]
SOS_IIR_Filter A_weighting = {
  gain: 0.169994948147430, 
  sos: { // Second-Order Sections {b1, b2, -a1, -a2}
    {-2.00026996133106, +1.00027056142719, -1.060868438509278, -0.163987445885926},
    {+4.35912384203144, +3.09120265783884, +1.208419926363593, -0.273166998428332},
    {-0.70930303489759, -0.29071868393580, +1.982242159753048, -0.982298594928989}
  }
};

//
// C-weighting IIR Filter, Fs = 48KHz 
// B = [-0.49164716933714026, 0.14844753846498662, 0.74117815661529129, -0.03281878334039314, -0.29709276192593875, -0.06442545322197900, -0.00364152725482682]
// A = [1.0, -1.0325358998928318, -0.9524000181023488, 0.8936404694728326   0.2256286147169398  -0.1499917107550188, 0.0156718181681081]
SOS_IIR_Filter C_weighting = {
  gain: -0.491647169337140,
  sos: { 
    {+1.4604385758204708, +0.5275070373815286, +1.9946144559930252, -0.9946217070140883},
    {+0.2376222404939509, +0.0140411206016894, -1.3396585608422749, -0.4421457807694559},
    {-2.0000000000000000, +1.0000000000000000, +0.3775800047420818, -0.0356365756680430}
  }
};

#endif /* MICROFONO_H */

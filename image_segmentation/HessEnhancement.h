/*This part is the realization of the heisen matrix
 * 2018-11-9 : by Lingsheng Kong
 */
#ifndef HessEnhancement_H
#define HessEnhancement_H

#if _MSC_VER > 1000
#pragma once
#endif 


class NeuronEnhancementFilter  
{
public:
	NeuronEnhancementFilter();
	virtual ~NeuronEnhancementFilter();

	int MultiScaleFilter3D(short *apsImage, float *apfFiltedImg, 
												 int iWidth, int iLength, int iHeight, 
												 int *apiSigma, int iSigmaLength,
												 float fA, float fB, float fC);
	
public:
	void Expand3DBoundary(short *apsImg, int iLayer, int iRow, int iColumn, short *apsImgNew);

	int Calculatelness(short *apfVess, short *apfFiltedImg, 
						int iSliceNmb, int iColNmb, int iRowNmb, float fA, float fB, float fC);

	void ConvolThreeDimension(short *apsImage, short *apfFiltedImg, float *apdTemplate, 
						int iRowNumber, int iColumnNumber, int iSliceNumber, int iConvLen);

	void CalEigenValue( float *Dxx_f, float *Dxy_f, float *Dxz_f, float *Dyy_f, float *Dyz_f, float *Dzz_f, 
				    /*float *Dvecx_f, float *Dvecy_f, float *Dvecz_f, */float *Deiga_f, float *Deigb_f, float *Deigc_f, 
				    int npixels); 

private:
	
	double hypot2(double x, double y);
	float absd(float val);
	
	void tred2(float V[3][3], float d[3], float e[3]);
	void tql2(float V[3][3], float d[3], float e[3]);
	void eigen_decomposition(float A[3][3], float V[3][3], float d[3]);

private:
	
};

#endif 
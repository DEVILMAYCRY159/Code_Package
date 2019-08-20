/*This code is the realization of image segmentation
 * 2018-11-8 : by Lingsheng Kong
 */
#ifndef NeuronSeg_H
#define NeuronSeg_H

#if _MSC_VER > 1000
#pragma once
#endif // 

#include <deque>
#include <vector> 
#include <fstream>
#include <iostream>
#include "NeuronEnhancementFilter.h"

using namespace std;

typedef struct{
	int x;
	int y;
	int z;
}Point3D_t;

class NeuronSeg  
{
public:
	NeuronSeg();
	virtual ~NeuronSeg();
	
public:
	void BinaryProcess(short *apsInput, int iImageWidth, int iImageHeight, int iImageLayer, short *apsOutput);
	
	void Neuron_Segmentation(short *apfImg, int *iSigma, int iRowNmb, int iColNmb, int iSliceNmb, 
		float fA, float fB, float fC,
		int iSigmaLen, short* apsVesMark, int iDelSize, int &iVesCnt, bool bBinaryPro, bool bVolFilter);

	void VoiSelect(short* apsInputData, short*apsImg, int iImageLayer, int iImageHeight, int iImageWidth,
			short* apsVesMark, int iDelSize, int &iVesCnt);

private:
	
	void VolumeFilterInitial(short* apsInputData, short *apsInputCopy, int  iSliceNmb, int  iRowNmb, int iColNmb);
	//		| iZ				
	//		|
	//		|
	//		--------> iX
    //     /
	//    / iY
	void Calculate26Adjacent(short * apsInputCopy, deque<Point3D_t> &dePointSetVessel,  deque<Point3D_t> &dePointSetTmp,
							 int iCurSlice,int iCurRowNmb,int iCurColNmb, int iRowNmb, int iColNmb, int &iCnt);

	//
	void CalculateRegionGrowAdjacent(short * apsInputCopy, deque<Point3D_t> &dePointSetVessel,  deque<Point3D_t> &dePointSetTmp,
							 int iCurSlice,int iCurRowNmb,int iCurColNmb, int iRowNmb, int iColNmb, int &iCnt);
private:
	
};

#endif // 

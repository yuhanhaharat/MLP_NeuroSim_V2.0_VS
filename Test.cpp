/*******************************************************************************
* Copyright (c) 2015-2017
* School of Electrical, Computer and Energy Engineering, Arizona State University
* PI: Prof. Shimeng Yu
* All rights reserved.
*
* This source code is part of NeuroSim - a device-circuit-algorithm framework to benchmark
* neuro-inspired architectures with synaptic devices(e.g., SRAM and emerging non-volatile memory).
* Copyright of the model is maintained by the developers, and the model is distributed under
* the terms of the Creative Commons Attribution-NonCommercial 4.0 International Public License
* http://creativecommons.org/licenses/by-nc/4.0/legalcode.
* The source code is free and you can redistribute and/or modify it
* by providing that the following conditions are met:
*
*  1) Redistributions of source code must retain the above copyright notice,
*     this list of conditions and the following disclaimer.
*
*  2) Redistributions in binary form must reproduce the above copyright notice,
*     this list of conditions and the following disclaimer in the documentation
*     and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
* Developer list:
*   Pai-Yu Chen     Email: pchen72 at asu dot edu
*
*   Xiaochen Peng   Email: xpeng15 at asu dot edu
********************************************************************************/

#include <cstdio>
#include <iostream>
#include <vector>
#include <random>
#include "formula.h"
#include "Param.h"
#include "Array.h"
#include "Mapping.h"
#include "NeuroSim.h"

extern Param *param;

extern std::vector< std::vector<double> > testInput;
extern std::vector< std::vector<int> > dTestInput;
extern std::vector< std::vector<double> > testOutput;

extern std::vector< std::vector<double> > weight1;
extern std::vector< std::vector<double> > weight2;

extern Technology techIH;
extern Technology techHO;
extern Array *arrayIH;
extern Array *arrayHO;
extern SubArray *subArrayIH;
extern SubArray *subArrayHO;
extern Adder adderIH;
extern Mux muxIH;
extern RowDecoder muxDecoderIH;
extern DFF dffIH;
extern Adder adderHO;
extern Mux muxHO;
extern RowDecoder muxDecoderHO;
extern DFF dffHO;

extern int correct;		// # of correct prediction

/* Validation */
void Validate()
{
	int numBatchReadSynapse;    // # of read synapses in a batch read operation (decide later)
	std::vector<double> outN1(param->nHide); // Net input to the hidden layer [param->nHide]
	std::vector<double> a1(param->nHide);    // Net output of hidden layer [param->nHide] also the input of hidden layer to output layer
	std::vector<int> da1(param->nHide);  // Digitized net output of hidden layer [param->nHide] also the input of hidden layer to output layer
	std::vector<double> outN2(param->nOutput);   // Net input to the output layer [param->nOutput]
	std::vector<double> a2(param->nOutput);  // Net output of output layer [param->nOutput]
	double tempMax;
	int countNum;
	correct = 0;

	double sumArrayReadEnergyIH = 0;   // Use a temporary variable here since OpenMP does not support reduction on class member
	double sumNeuroSimReadEnergyIH = 0;   // Use a temporary variable here since OpenMP does not support reduction on class member
	double sumReadLatencyIH = 0;    // Use a temporary variable here since OpenMP does not support reduction on class member
	double readVoltageIH = static_cast<eNVM*>(arrayIH->cell[0][0])->readVoltage;
	double readPulseWidthIH = static_cast<eNVM*>(arrayIH->cell[0][0])->readPulseWidth;
	double sumArrayReadEnergyHO = 0;    // Use a temporary variable here since OpenMP does not support reduction on class member
	double sumNeuroSimReadEnergyHO = 0; // Use a temporary variable here since OpenMP does not support reduction on class member
	double sumReadLatencyHO = 0;    // Use a temporary variable here since OpenMP does not support reduction on class member
	double readVoltageHO = static_cast<eNVM*>(arrayHO->cell[0][0])->readVoltage;
	double readPulseWidthHO = static_cast<eNVM*>(arrayHO->cell[0][0])->readPulseWidth;

#pragma omp parallel for private(outN1, a1, da1, outN2, a2, tempMax, countNum, numBatchReadSynapse) reduction(+: correct, sumArrayReadEnergyIH, sumNeuroSimReadEnergyIH, sumArrayReadEnergyHO, sumNeuroSimReadEnergyHO, sumReadLatencyIH, sumReadLatencyHO)
	for (int i = 0; i < param->numMnistTestImages; i++)
	{
		// Forward propagation
		/* First layer from input layer to the hidden layer */
		outN1 = std::vector<double>(param->nHide);
		a1 = std::vector<double>(param->nHide);
		da1 = std::vector<int>(param->nHide);
		if (param->useHardwareInTestingFF)
		{
			// Hardware
			for (int j = 0; j < param->nHide; j++)
			{
				if (AnalogNVM *temp = dynamic_cast<AnalogNVM*>(arrayIH->cell[0][0]))
				{
					// Analog eNVM
					if (static_cast<eNVM*>(arrayIH->cell[0][0])->cmosAccess)
					{
						// 1T1R
						sumArrayReadEnergyIH += arrayIH->wireGateCapRow * techIH.vdd * techIH.vdd * param->nInput; // All WLs open
					}
				}
				else if (DigitalNVM *temp = dynamic_cast<DigitalNVM*>(arrayIH->cell[0][0]))
				{
					// Digital eNVM
					if (static_cast<eNVM*>(arrayIH->cell[0][0])->cmosAccess)
					{
						// 1T1R
						sumArrayReadEnergyIH += arrayIH->wireGateCapRow * techIH.vdd * techIH.vdd;  // Selected WL
					}
					else
					{    // Cross-point
						sumArrayReadEnergyIH += arrayIH->wireCapRow * techIH.vdd * techIH.vdd * (param->nInput - 1);    // Unselected WLs
					}
				}
				for (int n = 0; n < param->numBitInput; n++)
				{
					double pSumMaxAlgorithm = pow(2, n) / (param->numInputLevel - 1) * arrayIH->arrayRowSize;   // Max algorithm partial weighted sum for the nth vector bit (if both max input value and max weight are 1)
					if (AnalogNVM *temp = dynamic_cast<AnalogNVM*>(arrayIH->cell[0][0]))
					{
						// Analog eNVM
						double Isum = 0;    // weighted sum current
						double IsumMax = 0; // Max weighted sum current
						double inputSum = 0;    // Weighted sum current of input vector * weight=1 column
						for (int k = 0; k < param->nInput; k++)
						{
							if ((dTestInput[i][k] >> n) & 1)
							{
								// if the nth bit of dTestInput[i][k] is 1
								Isum += arrayIH->ReadCell(j, k);
								inputSum += arrayIH->GetMaxCellReadCurrent(j, k);
								sumArrayReadEnergyIH += arrayIH->wireCapRow * readVoltageIH * readVoltageIH;   // Selected BLs (1T1R) or Selected WLs (cross-point)
							}
							IsumMax += arrayIH->GetMaxCellReadCurrent(j, k);
						}

						sumArrayReadEnergyIH += Isum * readVoltageIH * readPulseWidthIH;
						int outputDigits = 2 * CurrentToDigits(Isum, IsumMax) - CurrentToDigits(inputSum, IsumMax);
						outN1[j] += DigitsToAlgorithm(outputDigits, pSumMaxAlgorithm);
					}
					else
					{
						// SRAM or digital eNVM
						int Dsum = 0;
						int DsumMax = 0;
						int inputSum = 0;
						for (int k = 0; k < param->nInput; k++)
						{
							if ((dTestInput[i][k] >> n) & 1)
							{    // if the nth bit of dTestInput[i][k] is 1
								Dsum += (int)(arrayIH->ReadCell(j, k));
								inputSum += pow(2, arrayIH->numCellPerSynapse) - 1;
							}
							DsumMax += pow(2, arrayIH->numCellPerSynapse) - 1;
						}
						if (DigitalNVM *temp = dynamic_cast<DigitalNVM*>(arrayIH->cell[0][0]))
						{    // Digital eNVM
							sumArrayReadEnergyIH += static_cast<DigitalNVM*>(arrayIH->cell[0][0])->readEnergy * arrayIH->numCellPerSynapse * arrayIH->arrayRowSize;
						}
						else
						{
							sumArrayReadEnergyIH += static_cast<SRAM*>(arrayIH->cell[0][0])->readEnergy * arrayIH->numCellPerSynapse * arrayIH->arrayRowSize;
						}
						outN1[j] += (double)(2 * Dsum - inputSum) / DsumMax * pSumMaxAlgorithm;
					}
				}

				a1[j] = sigmoid(outN1[j]);
				//da1[j] = round(a1[j] * (param->numInputLevel - 1));
				da1[j] = round_th(a1[j] * (param->numInputLevel - 1), param->Hthreshold);
			}

			numBatchReadSynapse = (int)ceil((double)param->nHide / param->numColMuxed);

#pragma omp critical // Use critical here since NeuroSim class functions may update its member variables
			for (int j = 0; j < param->nHide; j += numBatchReadSynapse)
			{
				int numActiveRows = 0;  // Number of selected rows for NeuroSim
				for (int n = 0; n < param->numBitInput; n++)
				{
					for (int k = 0; k < param->nInput; k++)
					{
						if ((dTestInput[i][k] >> n) & 1)
						{
							// if the nth bit of dTestInput[i][k] is 1
							numActiveRows++;
						}
					}
				}
				subArrayIH->activityRowRead = (double)numActiveRows / param->nInput / param->numBitInput;
				sumNeuroSimReadEnergyIH += NeuroSimSubArrayReadEnergy(subArrayIH);
				sumNeuroSimReadEnergyIH += NeuroSimNeuronReadEnergy(subArrayIH, adderIH, muxIH, muxDecoderIH, dffIH);
				sumReadLatencyIH += NeuroSimSubArrayReadLatency(subArrayIH);
				sumReadLatencyIH += NeuroSimNeuronReadLatency(subArrayIH, adderIH, muxIH, muxDecoderIH, dffIH);
			}
		}
		else
		{
			// Algorithm
			for (int j = 0; j < param->nHide; j++)
			{
				for (int k = 0; k < param->nInput; k++)
				{
					outN1[j] += 2 * testInput[i][k] * weight1[j][k] - testInput[i][k];
				}
				a1[j] = sigmoid(outN1[j]);
			}
		}

		/* Second layer from hidden layer to the output layer */
		tempMax = 0;
		countNum = 0;
		outN2 = std::vector<double>(param->nOutput);
		a2 = std::vector<double>(param->nOutput);
		if (param->useHardwareInTestingFF)
		{
			// Hardware
			for (int j = 0; j < param->nOutput; j++)
			{
				if (AnalogNVM *temp = dynamic_cast<AnalogNVM*>(arrayHO->cell[0][0]))
				{
					// Analog eNVM
					if (static_cast<eNVM*>(arrayHO->cell[0][0])->cmosAccess)
					{
						// 1T1R
						sumArrayReadEnergyHO += arrayHO->wireGateCapRow * techHO.vdd * techHO.vdd * param->nHide; // All WLs open
					}
				}
				else if (DigitalNVM *temp = dynamic_cast<DigitalNVM*>(arrayHO->cell[0][0]))
				{
					if (static_cast<eNVM*>(arrayHO->cell[0][0])->cmosAccess)
					{
						// 1T1R
						sumArrayReadEnergyHO += arrayHO->wireGateCapRow * techHO.vdd * techHO.vdd;  // Selected WL
					}
					else
					{
						// Cross-point
						sumArrayReadEnergyHO += arrayHO->wireCapRow * techHO.vdd * techHO.vdd * (param->nHide - 1); // Unselected WLs
					}
				}
				for (int n = 0; n < param->numBitInput; n++)
				{
					double pSumMaxAlgorithm = pow(2, n) / (param->numInputLevel - 1) * arrayHO->arrayRowSize;    // Max algorithm partial weighted sum for the nth vector bit (if both max input value and max weight are 1)
					if (AnalogNVM *temp = dynamic_cast<AnalogNVM*>(arrayHO->cell[0][0]))
					{
						// Analog NVM
						double Isum = 0;    // weighted sum current
						double IsumMax = 0; // Max weighted sum current
						double a1Sum = 0;   // Weighted sum current of a1 vector * weight=1 column
						for (int k = 0; k < param->nHide; k++)
						{
							if ((da1[k] >> n) & 1)
							{
								// if the nth bit of da1[k] is 1
								Isum += arrayHO->ReadCell(j, k);
								a1Sum += arrayHO->GetMaxCellReadCurrent(j, k);
								sumArrayReadEnergyHO += arrayHO->wireCapRow * readVoltageHO * readVoltageHO;   // Selected BLs (1T1R) or Selected WLs (cross-point)
							}
							IsumMax += arrayHO->GetMaxCellReadCurrent(j, k);
						}
						sumArrayReadEnergyHO += Isum * readVoltageHO * readPulseWidthHO;
						int outputDigits = 2 * CurrentToDigits(Isum, IsumMax) - CurrentToDigits(a1Sum, IsumMax);
						outN2[j] += DigitsToAlgorithm(outputDigits, pSumMaxAlgorithm);
					}
					else
					{
						// SRAM or digital eNVM
						int Dsum = 0;
						int DsumMax = 0;
						int a1Sum = 0;
						for (int k = 0; k < param->nHide; k++)
						{
							if ((da1[k] >> n) & 1)
							{
								// if the nth bit of da1[k] is 1
								Dsum += (int)(arrayHO->ReadCell(j, k));
								a1Sum += pow(2, arrayHO->numCellPerSynapse) - 1;
							}
							DsumMax += pow(2, arrayHO->numCellPerSynapse) - 1;
						}
						if (DigitalNVM *temp = dynamic_cast<DigitalNVM*>(arrayHO->cell[0][0]))
						{
							// Digital eNVM
							sumArrayReadEnergyHO += static_cast<DigitalNVM*>(arrayHO->cell[0][0])->readEnergy * arrayHO->numCellPerSynapse * arrayHO->arrayRowSize;
						}
						else
						{
							sumArrayReadEnergyHO += static_cast<SRAM*>(arrayHO->cell[0][0])->readEnergy * arrayHO->numCellPerSynapse * arrayHO->arrayRowSize;
						}
						outN2[j] += (double)(2 * Dsum - a1Sum) / DsumMax * pSumMaxAlgorithm;
					}
				}
				a2[j] = sigmoid(outN2[j]);
				if (a2[j] > tempMax)
				{
					tempMax = a2[j];
					countNum = j;
				}
			}

			numBatchReadSynapse = (int)ceil((double)param->nOutput / param->numColMuxed);

#pragma omp critical // Use critical here since NeuroSim class functions may update its member variables
			for (int j = 0; j < param->nOutput; j += numBatchReadSynapse)
			{
				int numActiveRows = 0;
				// Number of selected rows for NeuroSim
				for (int n = 0; n < param->numBitInput; n++)
				{
					for (int k = 0; k < param->nHide; k++)
					{
						if ((da1[k] >> n) & 1)
						{
							// if the nth bit of da1[k] is 1
							numActiveRows++;
						}
					}
				}
				subArrayHO->activityRowRead = (double)numActiveRows / param->nHide / param->numBitInput;
				sumNeuroSimReadEnergyHO += NeuroSimSubArrayReadEnergy(subArrayHO);
				sumNeuroSimReadEnergyHO += NeuroSimNeuronReadEnergy(subArrayHO, adderHO, muxHO, muxDecoderHO, dffHO);
				sumReadLatencyHO += NeuroSimSubArrayReadLatency(subArrayHO);
				sumReadLatencyHO += NeuroSimNeuronReadLatency(subArrayHO, adderHO, muxHO, muxDecoderHO, dffHO);
			}
		}
		else
		{
			// Algorithm
			for (int j = 0; j < param->nOutput; j++)
			{
				for (int k = 0; k < param->nHide; k++)
				{
					outN2[j] += 2 * a1[k] * weight2[j][k] - a1[k];
				}
				a2[j] = sigmoid(outN2[j]);
				if (a2[j] > tempMax)
				{
					tempMax = a2[j];
					countNum = j;
				}
			}
		}
		if (testOutput[i][countNum] == 1)
		{
			correct++;
		}
	}
	if (!param->useHardwareInTraining)
	{
		// Calculate the classification latency and energy only for offline classification
		arrayIH->readEnergy += sumArrayReadEnergyIH;
		subArrayIH->readDynamicEnergy += sumNeuroSimReadEnergyIH;
		arrayHO->readEnergy += sumArrayReadEnergyHO;
		subArrayHO->readDynamicEnergy += sumNeuroSimReadEnergyHO;
		subArrayIH->readLatency += sumReadLatencyIH;
		subArrayHO->readLatency += sumReadLatencyHO;
	}
}


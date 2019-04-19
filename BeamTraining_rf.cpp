#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <fstream>
#include <algorithm>
#include <time.h>
#include <limits.h>

#include "iconv.h"
#include "pthread.h"
#include "speedtest.h"

#include "Common/mrloopsdkheader.h"
#include "mrloopbf_release.h"

#define BUFSIZE  4096

bool is_open, s_index = false, running = false;
long bits;

bool rx_rfstatus = false;
int pkts = 10, pkt_no = 0, retry_threshold = 10;
int RX_start = 0;

pthread_t thread;
pthread_t show_bitrate;
pthread_t getRFstatus;
ML_RF_INF ML_RF_Record;

void readRFPackets(ML_RF_INF Packets, int Layer, int BeamID)
{
	uint32_t n_detect = Packets.PHY_Rx_SC_PKT + Packets.PHY_Rx_CP_PKT;
	uint32_t n_pass = Packets.PHY_Total_Rx_Count - Packets.PHY_RX_FCS_Err;
	uint32_t n_error = n_detect - n_pass;
	if (n_detect == 0)	n_detect = 1;
	
	fprintf(stdout, "\033[2J"); //clear Screen
	fprintf(stdout, "\033[9;0H Phase %d, BeamID: %d\n", Layer, BeamID);
	fprintf(stdout, "\033[10;0H PHY COUNTERS: \n");
	fprintf(stdout, "\033[11;3H Total Tx Counter: %u\n", Packets.PHY_Total_Tx_Count);
	fprintf(stdout, "\033[12;3H Total Rx Counter: %u\n", Packets.PHY_Total_Rx_Count);
	fprintf(stdout, "\033[13;3H Rx CP PKT: %u\n", Packets.PHY_Rx_CP_PKT);
	fprintf(stdout, "\033[14;3H Rx SC PKT: %u\n", Packets.PHY_Rx_SC_PKT);

	fprintf(stdout, "\033[15;3H PER: %1.3f\n", (float)n_error / (float)n_detect);
	fprintf(stdout, "\033[16;3H RX STF: %1.3f\n", (float)Packets.PHY_Rx_STF_Err / (float)n_detect);
	fprintf(stdout, "\033[17;3H RX HCS: %1.3f\n", (float)Packets.PHY_Rx_HCS_Err / (float)n_detect);
	fprintf(stdout, "\033[18;3H RX FCS: %1.3f\n\n", (float)Packets.PHY_RX_FCS_Err / (float)n_detect);

	fprintf(stdout, "\033[11;40H RX EVM(dBm): %d\n", Packets.PHY_Rx_EVM);
	fprintf(stdout, "\033[12;40H RX SNR(dBm): %d\n", Packets.PHY_RX_SNR);
	fprintf(stdout, "\033[13;40H RX RSSI(dBm): %d\n", Packets.PHY_RSSI);
	fprintf(stdout, "\033[14;40H RX RCPI(dBm): %d\n", Packets.PHY_RCPI);
	fprintf(stdout, "\033[15;40H AGC GAIN: %d\n\n", Packets.PHY_AGC_Gain);
	fprintf(stdout, "\033[16;40H MCS: %d\n\n", Packets.MCS);

	fprintf(stdout, "\033[20;0H MAC COUNTERS: \n");
	fprintf(stdout, "\033[22;3H Total Tx: %u\n", Packets.MAC_Tx_Total);
	fprintf(stdout, "\033[22;3H Total Rx: %u\n", Packets.MAC_Rx_Total);
	fprintf(stdout, "\033[23;3H Total Fail: %u\n", Packets.MAC_Total_Fail);
	fprintf(stdout, "\033[24;3H Total Ack: %u\n", Packets.MAC_Total_Ack);
	fprintf(stdout, "\033[25;3H Total Tx Done: %u\n\n", Packets.MAC_Total_Tx_Done);

	uint32_t correct_packets = (1 - ((float)n_error / (float)n_detect))*pkts * 4096 * 8;
	uint32_t tx_time = pkts * 4096 * 8 / (1925);
	fprintf(stdout, "\033[26;3H Throughput: %1.3f Mbps\n", (float)correct_packets / (float)tx_time);
	
	//fprintf(stdout,"\033[2J"); //clear Screen
	//*beam_snr_ptr = (int)Packets.PHY_RX_SNR;
	//std::fstream fs;
	//fs.open("test.txt", std::fstream::out | std::fstream::app);
	//fs<< "PHY_RX_SNR_ptr:" << *beam_snr_ptr << std::endl;
	//fs<< "PHY_RX_SNR:" << (int)Packets.PHY_RX_SNR << std::endl;
	//beam_snr_ptr++;
	//fs.close();
}

void TxGetRF(int *ptr, int flag_ack, int Layer, int BeamID)
{
	uint8_t *buf = new uint8_t[BUFSIZE];
	int length;
	static int *beam_ack_ptr = (int *)ptr;
	if (flag_ack == 0)	beam_ack_ptr = (int *)ptr;

	std::fstream fs;
	fs.open("tx_record_ack.txt", std::fstream::out | std::fstream::app);
	
	//ML_GetRFStatus(&ML_RF_Record);
	length = BUFSIZE;
	/*Send request to RF*/
	ML_SendRFStatusReq();
	/*Get RF data buf*/
	ML_Receiver(buf, &length);

	/*Decode RF status packet*/
	if (ML_DecodeRFStatusPacket(buf, &ML_RF_Record)) {
	//	readRFPackets(ML_RF_Record, Layer, BeamID);
		*beam_ack_ptr = ML_RF_Record.MAC_Total_Ack;
	}
	else {
		fs << "Tx fail" << std::endl;
		*beam_ack_ptr = 0;
	}

	sleep(1);
	fs << "MAX_TX_ACK:" << *beam_ack_ptr << std::endl;
	fs << "--------------------------" << std::endl;
	beam_ack_ptr++; //ONLY record the last packets in the same beam trail
	free(buf);
}

void RxGetRF()
{
	if (!rx_rfstatus)
		rx_rfstatus = true;

	//sleep(1);
}

int main(int argc, char *argv[])
{
	bool tx_idx = false, rx_idx = false;

#if 1 
	//int menu, tmp_num, tmp_sp, tmp_rule, tmp_tx_sb, tmp_rx_sb;
	//uint8_t *speed = NULL, *rule = NULL, *tx_beam = NULL, *rx_beam = NULL;
	int menu, tmp_num, tmp_rule;
	int beam_ack_phase1[9][9] = {0}, beam_ack_div_phase1[9][9] = {0}; //Store the # of acks(Phase 1) //1.21
	//int beam_ack_phase2_part1[3][3] = {0}, beam_ack_div_phase2_part1[3][3] = {0}; //Store the # of acks(Phase 2) //1.21
	//int beam_ack_phase2_part2[3][3] = {0}, beam_ack_div_phase2_part2[3][3] = {0}; //Store the # of acks(Phase 2) //1.21
	//int beam_ack_phase3_part1[2][2] = {0}, beam_ack_div_phase3_part1[2][2] = {0}; //Store the # of acks(Phase 3) //1.21
	//int beam_ack_phase3_part2[2][2] = {0}, beam_ack_div_phase3_part2[2][2] = {0}; //Store the # of acks(Phase 3) //1.21
	
	int beam_snr_phase1[9][9] = {0}; //Store the SNR(Phase 1) //1.21
	//int beam_snr_phase2_part1[3][3] = {0}, beam_snr_phase2_part2[3][3] = {0}; //Store the SNR(Phase 2)           //1.21
	//int beam_snr_phase3_part1[2][2] = {0}, beam_snr_phase3_part2[2][2] = {0}; //Store the SNR(Phase 3)*/         //1.21
	int Layer, BeamID;
	uint8_t *rule = NULL;

	while (true) {
		do {
			fprintf(stdout, "\033[2J"); //clear Screen
			fprintf(stdout, "\033[1;0H");
			if (tx_idx) {
				fprintf(stdout, "Select Function: \n");
				fprintf(stdout, "        1. Speed Set\n");
				fprintf(stdout, "        0. Quit\n");
			}
			else if (rx_idx) {
				fprintf(stdout, "Select Function: \n");
				fprintf(stdout, "        0. Quit\n");
			}
			else {
				fprintf(stdout, "Select Function: \n");
				fprintf(stdout, "	1. Rule Set\n");
				fprintf(stdout, "	2. Start BeamTraining\n");
				fprintf(stdout, "	0. Quit\n");
			}
			scanf("%d", &menu);

			switch (menu) {
			case 1:
				if (!is_open) {
					fprintf(stdout, "Select Rule : 1. PCP/AP, 2. STA, 3.Non-PCP/Non STA\n");
					scanf("%d", &tmp_rule);
					rule = (uint8_t*)&tmp_rule;
				}
				break;
			case 2:
#if 1
				if (!running) {
					if (rule != NULL) {
						if (ML_Init() != 1) {
							return 0;
						}
						else {
							ML_SetMode(*rule);
							is_open = true;
							fprintf(stdout, "Select Function 1. Tx, 2.Rx\n");
							scanf("%d", &tmp_num);
							if (tmp_num == 1) {
								fprintf(stdout, "Beam Training Start!\n");
								s_index = true;
								running = true;
								tx_idx = true;

								std::fstream fs;
								fs.open("tx_result_main.txt", std::fstream::out | std::fstream::app);

								int mcs_record = 7; //Initial MCS mod=7;
								//1.21 (start fix)
								//Phase 1 of Beam Training
								int beam_phase1[9] = { 1,2,3,4,5,6,7,8,9 }; //1.21
								int flag_ack = 0;
								int *beam_ack_ptr = (int *)beam_ack_phase1; //pointer to beam_ack_phase1
								int exhaustive_search_result_tx_t = 0; //new variable(1.21)
								int exhaustive_search_result_rx_t = 0; //new variable(1.21)
								
								for (int i = 0; i < 9; i++) {
									for (int j = 0; j < 9; j++) {
										ML_Init(); //Initial the device and SDK first.
										ML_SetSpeed(mcs_record); 
										ML_SetTxSector(beam_phase1[i]);
										fs << "TX_Beam ID:" << beam_phase1[i] << std::endl;
										Layer = 1;
										BeamID = beam_phase1[i];
										pthread_create(&show_bitrate, NULL, ShowBitrate, NULL);
										SpeedTx(BeamID);
										TxGetRF(beam_ack_ptr, flag_ack, Layer, BeamID);
										flag_ack = 1;
										ML_Close(); //Close the device and leave SDK
									}
								}
								//Print Results		(not important) 1.21								
								for (int i = 0; i < 9; i++) {
									for (int j = 0; j < 9; j++) {
										fs << "beam_ACK_phase1[" << i << "][" << j << "]: " << beam_ack_phase1[i][j] << std::endl;
									}
								}
								//Reshape the beam record matrix
								for (int p = 0; p < 9; p++) {
									for (int q = 0; q < 9; q++) {
									    //since the acc is accumlated, we should substract the privious one to get the interval acc (2019/1/21)
										if (p == 0 && q == 0)	beam_ack_div_phase1[p][q] = beam_ack_phase1[p][q];
										else if (p >= 1 && q == 0)	beam_ack_div_phase1[p][q] = beam_ack_phase1[p][q] - beam_ack_phase1[p - 1][9];
										else	beam_ack_div_phase1[p][q] = beam_ack_phase1[p][q] - beam_ack_phase1[p][q - 1];
										//Notice
										if (beam_ack_div_phase1[p][q] > pkts) beam_ack_div_phase1[p][q] = pkts;
									}
								}
								//Print Reshape-Beam Results									
								//Compare the # of acks and choose the highest
								int tmp1 = 0, count_tx_phase1[9] = { 0 }, record_count_tx_phase1[9] = { 0 }, tx_beam_id_phase1[2] = { 0 };
								for (int i = 0; i < 9; i++) {  //i tx
									for (int j = 0; j < 9; j++) {  //j rx 
										fs << "Reshape Beam[" << i << "][" << j << "](beam_ack_div_phase1): " << beam_ack_div_phase1[i][j] << std::endl;
										if (beam_ack_div_phase1[i][j] > tmp1) {
											tmp1 = beam_ack_div_phase1[i][j];      //maybe this is what I want(tx) (2019.1.21)
											exhaustive_search_result_tx_t = i+1;
											exhaustive_search_result_rx_t = j+1;
										}
									}
								}
								
								/*
                                
								//Select 2 best beam sector
								//maybe this secector can be combine in (240,241,242) (1.21)
								for (int i = 0; i < 4; i++) {
									for (int j = 0; j < 4; j++) {
										if (beam_ack_div_phase1[i][j] == tmp1) {
											count_tx_phase1[i]++;
											record_count_tx_phase1[i]++;   // this is wired here (1.21)
										}
									}
								}
								for (int i = 0; i < 4; i++) fs << "count_tx_phase1[" << i << "]:" << count_tx_phase1[i] << std::endl;
								std::sort(record_count_tx_phase1, record_count_tx_phase1 + 4);
								std::reverse(record_count_tx_phase1, record_count_tx_phase1 + 4); //big to small
								for (int i = 0; i < 4; i++) fs << "record_count_tx_phase1(sort)[" << i << "]:" << record_count_tx_phase1[i] << std::endl;
								
								for (int num = 0; num < 2; num++) {
									for (int k = 0; k < 4; k++) {
										if (num == 0 && count_tx_phase1[k] == record_count_tx_phase1[num]) {
											tx_beam_id_phase1[num] = k;
										}
										else if (count_tx_phase1[k] == record_count_tx_phase1[num] && k != tx_beam_id_phase1[num - 1]) {
											tx_beam_id_phase1[num] = k;
										}
									}
								}
								//Print the result of phase 1 in beam training
								for (int num = 0; num < 2; num++) fs << "Tx_beam_phase1: Sector " << beam_phase1[tx_beam_id_phase1[num]] << std::endl;
								
								*/
								
                                //the code will be commend here (stard) 2019.1.21
				                /*
								//Phase 2-1 of Beam Training
								int candidate_phase2[4][3] = { { 1,2,4 },{ 4,5,6 },{ 7,8,9 },{ 9,10,1 } };
								flag_ack = 0;
								beam_ack_ptr = (int *)beam_ack_phase2_part1; //pointer to beam_ack_phase2
								int tmp2_p1 = 0, tx_beam_phase2_part1 = 0, record_tx_id_phase2_part1 = 0, count_tx_phase2_part1[3] = { 0 }, record_count_tx_phase2_part1[3] = { 0 };
								for (int i = 0; i < 3; i++) {
									for (int j = 0; j < 3; j++) {
										ML_Init(); //Initial the device and SDK first.
										ML_SetSpeed(mcs_record);
										ML_SetTxSector(candidate_phase2[tx_beam_id_phase1[0]][i]);
										fs << "TX_Beam ID:" << candidate_phase2[tx_beam_id_phase1[0]][i] << std::endl;
										Layer = 2;
										BeamID = candidate_phase2[tx_beam_id_phase1[0]][i];
										pthread_create(&show_bitrate, NULL, ShowBitrate, NULL);
										SpeedTx(BeamID);
										TxGetRF(beam_ack_ptr, flag_ack, Layer, BeamID);
										flag_ack = 1;
										ML_Close(); //Close the device and leave SDK
									}
								}
								//Print Results										
								for (int i = 0; i < 3; i++) {
									for (int j = 0; j < 3; j++) {
										fs << "beam_ACK_phase2_part1[" << i << "][" << j << "]: " << beam_ack_phase2_part1[i][j] << std::endl;
									}
								}
								//Reshape the beam record matrix
								for (int p = 0; p < 3; p++) {
									for (int q = 0; q < 3; q++) {
										if (p == 0 && q == 0)	beam_ack_div_phase2_part1[p][q] = beam_ack_phase2_part1[p][q] - beam_ack_phase1[3][3]; 
										else if (p >= 1 && q == 0)	beam_ack_div_phase2_part1[p][q] = beam_ack_phase2_part1[p][q] - beam_ack_phase2_part1[p - 1][2];
										else	beam_ack_div_phase2_part1[p][q] = beam_ack_phase2_part1[p][q] - beam_ack_phase2_part1[p][q - 1];
										//Notice
										if (beam_ack_div_phase2_part1[p][q] > pkts) beam_ack_div_phase2_part1[p][q] = pkts;
									}
								}
								//Print Reshape-Beam Results	
								//Compare the # of acks and choose the highest
								for (int i = 0; i < 3; i++) {
									for (int j = 0; j < 3; j++) {
										fs << "Reshape Beam[" << i << "][" << j << "](beam_ack_div_phase2_part1): " << beam_ack_div_phase2_part1[i][j] << std::endl;
										if (beam_ack_div_phase2_part1[i][j]>tmp2_p1) {
											tmp2_p1 = beam_ack_div_phase2_part1[i][j];
										}
									}
								}

								//Select optimal beam sector
								for (int i = 0; i < 3; i++) {
									for (int j = 0; j < 3; j++) {
										if (beam_ack_div_phase2_part1[i][j] == tmp2_p1) {
											count_tx_phase2_part1[i]++;
											record_count_tx_phase2_part1[i]++;
										}
									}
								}
								for (int i = 0; i < 3; i++) fs << "count_tx_phase2[" << i << "]:" << count_tx_phase2_part1[i] << std::endl;
								std::sort(record_count_tx_phase2_part1, record_count_tx_phase2_part1 + 3);
								std::reverse(record_count_tx_phase2_part1, record_count_tx_phase2_part1 + 3);
								for (int i = 0; i < 3; i++) fs << "record_count_tx_phase1(sort)[" << i << "]:" << record_count_tx_phase2_part1[i] << std::endl;
								for (int num = 0; num < 1; num++) {
									for (int k = 0; k < 3; k++) {
										if (count_tx_phase2_part1[k] == record_count_tx_phase2_part1[num]) {
											tx_beam_phase2_part1 = candidate_phase2[tx_beam_id_phase1[0]][k];
											record_tx_id_phase2_part1 = k;
										}
									}
								}

								//Phase 2-2 of Beam Training
								flag_ack = 0;
								beam_ack_ptr = (int *)beam_ack_phase2_part2; //pointer to beam_ack_phase2
								int tmp2_p2 = 0, tx_beam_phase2_part2 = 0, record_tx_id_phase2_part2 = 0, count_tx_phase2_part2[3] = { 0 }, record_count_tx_phase2_part2[3] = { 0 };
								for (int i = 0; i < 3; i++) {
									for (int j = 0; j < 3; j++) {
										ML_Init(); //Initial the device and SDK first.
										ML_SetSpeed(mcs_record);
										ML_SetTxSector(candidate_phase2[tx_beam_id_phase1[1]][i]);
										fs << "TX_Beam ID:" << candidate_phase2[tx_beam_id_phase1[1]][i] << std::endl;
										Layer = 2;
										BeamID = candidate_phase2[tx_beam_id_phase1[1]][i];
										pthread_create(&show_bitrate, NULL, ShowBitrate, NULL);
										SpeedTx(BeamID);
										TxGetRF(beam_ack_ptr, flag_ack, Layer, BeamID);
										flag_ack = 1;
										ML_Close(); //Close the device and leave SDK
									}
								}
								
								//Print Results										
								for (int i = 0; i < 3; i++) {
									for (int j = 0; j < 3; j++) {
										fs << "beam_ACK_phase2_part2[" << i << "][" << j << "]: " << beam_ack_phase2_part2[i][j] << std::endl;
									}
								}
								//Reshape the beam record matrix
								for (int p = 0; p < 3; p++) {
									for (int q = 0; q < 3; q++) {
										if (p == 0 && q == 0)	beam_ack_div_phase2_part2[p][q] = beam_ack_phase2_part2[p][q] - beam_ack_phase2_part1[2][2];
										else if (p >= 1 && q == 0)	beam_ack_div_phase2_part2[p][q] = beam_ack_phase2_part2[p][q] - beam_ack_phase2_part2[p - 1][2];
										else	beam_ack_div_phase2_part2[p][q] = beam_ack_phase2_part2[p][q] - beam_ack_phase2_part2[p][q - 1];
										//Notice
										if (beam_ack_div_phase2_part2[p][q] > pkts) beam_ack_div_phase2_part2[p][q] = pkts;
									}
								}
								//Print Reshape-Beam Results	
								//Compare the # of acks and choose the highest
								for (int i = 0; i < 3; i++) {
									for (int j = 0; j < 3; j++) {
										fs << "Reshape Beam[" << i << "][" << j << "](beam_ack_div_phase2_part2): " << beam_ack_div_phase2_part2[i][j] << std::endl;
										if (beam_ack_div_phase2_part2[i][j]>tmp2_p2) {
											tmp2_p2 = beam_ack_div_phase2_part2[i][j];
										}
									}
								}
								
								
								//Select optimal beam sector
								for (int i = 0; i < 3; i++) {
									for (int j = 0; j < 3; j++) {
										if (beam_ack_div_phase2_part2[i][j] == tmp2_p2) {
											count_tx_phase2_part2[i]++;
											record_count_tx_phase2_part2[i]++;
										}
									}
								}
								for (int i = 0; i < 3; i++) fs << "count_tx_phase2[" << i << "]:" << count_tx_phase2_part2[i] << std::endl;
								std::sort(record_count_tx_phase2_part2, record_count_tx_phase2_part2 + 3);
								std::reverse(record_count_tx_phase2_part2, record_count_tx_phase2_part2 + 3);
								for (int i = 0; i < 3; i++) fs << "record_count_tx_phase1(sort)[" << i << "]:" << record_count_tx_phase2_part2[i] << std::endl;
								for (int num = 0; num < 1; num++) {
									for (int k = 0; k < 3; k++) {
										if (count_tx_phase2_part2[k] == record_count_tx_phase2_part2[num]) {
											tx_beam_phase2_part2 = candidate_phase2[tx_beam_id_phase1[1]][k];
											record_tx_id_phase2_part2 = k;
										}
									}
								}
								//Print the result of phase 2 in beam training
								fs << "End Beam Training Phase 2!!" << std::endl;
								fs << "Tx_beam_phase2: Sector " << tx_beam_phase2_part1 << " and Sector " << tx_beam_phase2_part2 << std::endl;

								//Phase 3-1 of Beam Training
								int candidate_phase3[2][4][3] = { { { 1,2,4 },{ 4,4,5 },{ 5,5,8 },{ 9,1,1 } },{ { 2,4,5 },{ 5,5,6 },{ 7,8,9 },{ 10,9,10 } } };
								flag_ack = 0;
								beam_ack_ptr = (int *)beam_ack_phase3_part1; //pointer to beam_ack_phase3
								int tmp3_p1 = 0;
								for (int i = 0; i < 2; i++) {
									for (int j = 0; j < 2; j++) {
										ML_Init(); //Initial the device and SDK first.
										ML_SetSpeed(mcs_record);
										ML_SetTxSector(candidate_phase3[i][tx_beam_id_phase1[0]][record_tx_id_phase2_part1]);
										fs << "TX_Beam ID:" << candidate_phase3[i][tx_beam_id_phase1[0]][record_tx_id_phase2_part1] << std::endl;
										Layer = 3;
										BeamID = candidate_phase3[i][tx_beam_id_phase1[0]][record_tx_id_phase2_part1];
										pthread_create(&show_bitrate, NULL, ShowBitrate, NULL);
										SpeedTx(BeamID);
										TxGetRF(beam_ack_ptr, flag_ack, Layer, BeamID);
										flag_ack = 1;
										ML_Close(); //Close the device and leave SDK
									}
								}
								//Print Results										
								for (int i = 0; i < 2; i++) {
									for (int j = 0; j < 2; j++) {
										fs << "beam_ACK_phase3_part1[" << i << "][" << j << "]: " << beam_ack_phase3_part1[i][j] << std::endl;
									}
								}
								//Reshape the beam record matrix
								for (int p = 0; p < 2; p++) {
									for (int q = 0; q < 2; q++) {
										if (p == 0 && q == 0)	beam_ack_div_phase3_part1[p][q] = beam_ack_phase3_part1[p][q] - beam_ack_phase2_part2[2][2]; 
										else if (p >= 1 && q == 0)	beam_ack_div_phase3_part1[p][q] = beam_ack_phase3_part1[p][q] - beam_ack_phase3_part1[p - 1][1];
										else	beam_ack_div_phase3_part1[p][q] = beam_ack_phase3_part1[p][q] - beam_ack_phase3_part1[p][q - 1];
										//Notice
										if (beam_ack_div_phase3_part1[p][q] > pkts) beam_ack_div_phase3_part1[p][q] = pkts;
									}
								}
								//Print Reshape-Beam Results										
								//Compare the # of acks and choose the highest
								for (int i = 0; i < 2; i++) {
									for (int j = 0; j < 2; j++) {
										fs << "Reshape Beam[" << i << "][" << j << "](beam_ack_div_phase3_part1): " << beam_ack_div_phase3_part1[i][j] << std::endl;
										if (beam_ack_div_phase3_part1[i][j]>tmp3_p1) {
											tmp3_p1 = beam_ack_div_phase3_part1[i][j];
										}
									}
								}

								//Phase 3-2 of Beam Training
								flag_ack = 0;
								beam_ack_ptr = (int *)beam_ack_phase3_part2; //pointer to beam_ack_phase2
								int tmp3_p2 = 0;
								for (int i = 0; i < 2; i++) {
									for (int j = 0; j < 2; j++) {
										ML_Init(); //Initial the device and SDK first.
										ML_SetSpeed(mcs_record);
										ML_SetTxSector(candidate_phase3[i][tx_beam_id_phase1[1]][record_tx_id_phase2_part2]);
										fs << "TX_Beam ID:" << candidate_phase3[i][tx_beam_id_phase1[1]][record_tx_id_phase2_part2] << std::endl;
										Layer = 3;
										BeamID = candidate_phase3[i][tx_beam_id_phase1[1]][record_tx_id_phase2_part2];
										pthread_create(&show_bitrate, NULL, ShowBitrate, NULL);
										SpeedTx(BeamID);
										TxGetRF(beam_ack_ptr, flag_ack, Layer, BeamID);
										flag_ack = 1;
										ML_Close(); //Close the device and leave SDK
									}
								}
								//Print Results										
								for (int i = 0; i < 2; i++) {
									for (int j = 0; j < 2; j++) {
										fs << "beam_ACK_phase3_part2[" << i << "][" << j << "]: " << beam_ack_phase3_part2[i][j] << std::endl;
									}
								}
								//Reshape the beam record matrix
								for (int p = 0; p < 2; p++) {
									for (int q = 0; q < 2; q++) {
										if (p == 0 && q == 0)	beam_ack_div_phase3_part2[p][q] = beam_ack_phase3_part2[p][q] - beam_ack_phase3_part1[1][1]; 
										else if (p >= 1 && q == 0)	beam_ack_div_phase3_part2[p][q] = beam_ack_phase3_part2[p][q] - beam_ack_phase3_part2[p - 1][1];
										else	beam_ack_div_phase3_part1[p][q] = beam_ack_phase3_part2[p][q] - beam_ack_phase3_part2[p][q - 1];
										//Notice
										if (beam_ack_div_phase3_part2[p][q] > pkts) beam_ack_div_phase3_part2[p][q] = pkts;
									}
								}
								//Print Reshape-Beam Results										
								//Compare the # of acks and choose the highest
								for (int i = 0; i < 2; i++) {
									for (int j = 0; j < 2; j++) {
										fs << "Reshape Beam[" << i << "][" << j << "](beam_ack_div_phase3_part2): " << beam_ack_div_phase3_part2[i][j] << std::endl;
										if (beam_ack_div_phase3_part2[i][j]>tmp3_p2) {
											tmp3_p2 = beam_ack_div_phase3_part2[i][j];
										}
									}
								}
                                //the code will be commend here (end) 2019.1.21
                                */
								
								//Select the best beam sector ID
								/*
								int count_tx_phase3[2] = { 0 }, record_count_tx_phase3[2] = { 0 }, tx_beam_phase3 = 0;
								if (tmp3_p1>tmp3_p2) {
									for (int i = 0; i < 2; i++) {
										for (int j = 0; j < 2; j++) {
											if (beam_ack_div_phase3_part1[i][j] == tmp3_p1) {
												count_tx_phase3[i]++;
												record_count_tx_phase3[i]++;
											}
										}
									}
									for (int i = 0; i < 2; i++) fs << "count_tx_phase3_part1[" << i << "]:" << count_tx_phase3[i] << std::endl;
									std::sort(record_count_tx_phase3, record_count_tx_phase3 + 2);
									std::reverse(record_count_tx_phase3, record_count_tx_phase3 + 2);
									for (int i = 0; i < 2; i++) fs << "record_count_tx_phase3_part1(sort)[" << i << "]:" << record_count_tx_phase3[i] << std::endl;

									for (int num = 0; num < 1; num++) {
										for (int k = 0; k < 2; k++) {
											if (count_tx_phase3[k] == record_count_tx_phase3[num]) {
												tx_beam_phase3 = candidate_phase3[k][tx_beam_id_phase1[0]][record_tx_id_phase2_part1];
												//record_rx_id_phase3 = k;
											}
										}
									}
								}
								else {
									for (int i = 0; i < 2; i++) {
										for (int j = 0; j < 2; j++) {
											if (beam_ack_div_phase3_part2[i][j] == tmp3_p2) {
												count_tx_phase3[i]++;
												record_count_tx_phase3[i]++;
											}
										}
									}
									for (int i = 0; i < 2; i++) fs << "count_tx_phase3_part2[" << i << "]:" << count_tx_phase3[i] << std::endl;
									std::sort(record_count_tx_phase3, record_count_tx_phase3 + 2);
									std::reverse(record_count_tx_phase3, record_count_tx_phase3 + 2);
									for (int i = 0; i < 2; i++) fs << "record_count_tx_phase3_part2(sort)[" << i << "]:" << record_count_tx_phase3[i] << std::endl;

									for (int num = 0; num < 1; num++) {
										for (int k = 0; k < 2; k++) {
											if (count_tx_phase3[k] == record_count_tx_phase3[num]) {
												tx_beam_phase3 = candidate_phase3[k][tx_beam_id_phase1[1]][record_tx_id_phase2_part2];
												//record_rx_id_phase3 = k;
											}
										}
									}

								}
								*/
								//Print the result of phase 3(Finish) in beam training
								
								printf("==========search result(tx)========== \n");
								printf("exhaustive_search_result_tx_t         : %d \n",exhaustive_search_result_tx_t);
								printf("exhaustive_search_result_rx_t(suppose): %d \n",exhaustive_search_result_rx_t);
								printf("===================================== \n");
								
								fprintf(stdout, "\033[27;3H End Beam Training \n");
								fprintf(stdout, "\033[28;3H Best Tx_beam: %d\n\n", exhaustive_search_result_tx_t);   // print the best best beem (tx_beam_phase3)
								fs << "End Beam Training Phase 3!!" << std::endl;
								fs << "Tx_beam_final: Sector " << exhaustive_search_result_tx_t << std::endl;
								fs.close();
								
							}
							else if (tmp_num == 2) {
								fprintf(stdout, "Beam Training Start!\n");
								s_index = true;
								running = true;
								rx_idx = true;

								std::fstream fs;
								fs.open("rx_result_main.txt", std::fstream::out | std::fstream::app);

								int mcs_record = 7;
								ML_SetSpeed(mcs_record); //Initial MCS mod=7;
						
								//Phase 1 of Beam Training
								int beam_phase1[9] = { 1,2,3,4,5,6,7,8,9 };
								int flag_snr = 0;
								int *beam_snr_ptr = (int *)beam_snr_phase1; //pointer to beam_snr_phase1
								int pkt_num_synchr = -1; // resynch in 90 pkt
                               					int exhaustive_search_result_tx_r = 0; //new variable(1.21)
								int exhaustive_search_result_rx_r = 0; //new variable(1.21)
								int j=0;
								for (int i = 0; i < 9; i++) {
										ML_Init(); //Initial the device and SDK first.
										ML_SetSpeed(mcs_record);
										ML_SetRxSector(beam_phase1[j]);
										fs << "RX_Beam ID:" << beam_phase1[j] << std::endl;
										Layer = 1;
										BeamID = beam_phase1[j];
										pthread_create(&show_bitrate, NULL, ShowBitrate, NULL);
										j = SpeedRx(beam_snr_ptr, flag_snr, Layer, BeamID);
										RxGetRF();
										// (1.21)
										//if (pkt_num_synchr == -1) ;
											
										//else
										//	j = (int)pkt_num_synchr / 10;
                                                                                printf("===============beam change=============== \n");
                                                                                printf("Beam should ID : %d \n", (int)pkt_num_synchr / 10);
										flag_snr = 1;
										printf("pkt_no : %d \n", pkt_num_synchr);
                                                                                printf("===============beam change=============== \n");
										ML_Close(); //Close the device and leave SDK
								}
								//Print Results	and choose the highest snr
								int tmp1 = INT_MIN, count_rx_phase1[9] = { 0 }, record_count_rx_phase1[9] = { 0 }, rx_beam_id_phase1[2] = { 0 };;
								for (int i = 0; i < 9; i++) {
									for (int j = 0; j < 9; j++) {
										fs << "beam_snr_phase1[" << i << "][" << j << "]: " << beam_snr_phase1[i][j] << std::endl;
										if (beam_snr_phase1[i][j] > tmp1) {
											tmp1 = beam_snr_phase1[i][j];        //maybe this is what I want (rx)(1.21)
											exhaustive_search_result_tx_r = i+1;
											exhaustive_search_result_rx_r = j+1;
										}
									}
								}
								
								
								//Select 2 best beam sector
								/*
								for (int i = 0; i < 4; i++) {
									for (int j = 0; j < 4; j++) {
										if (beam_snr_phase1[i][j] == tmp1) {
											count_rx_phase1[j]++;
											record_count_rx_phase1[j]++;
										}
									}
								}
								for (int i = 0; i < 4; i++) fs << "count_rx_phase1[" << i << "]:" << count_rx_phase1[i] << std::endl;
								std::sort(record_count_rx_phase1, record_count_rx_phase1 + 4);
								std::reverse(record_count_rx_phase1, record_count_rx_phase1 + 4);
								for (int i = 0; i < 4; i++) fs << "record_count_rx_phase1(sort)[" << i << "]:" << record_count_rx_phase1[i] << std::endl;
								for (int num = 0; num < 2; num++) {
									for (int k = 0; k < 4; k++) {
										if (num == 0 && count_rx_phase1[k] == record_count_rx_phase1[num]) {
											rx_beam_id_phase1[num] = k;
										}
										else if (count_rx_phase1[k] == record_count_rx_phase1[num] && k != rx_beam_id_phase1[num - 1]) {
											rx_beam_id_phase1[num] = k;
										}
									}
								}
								//Print the result of phase 1 in beam training
								for (int num = 0; num < 2; num++) fs << "Rx_beam_phase1: Sector " << beam_phase1[rx_beam_id_phase1[num]] << std::endl;
							    */
								
								//the code will be commend here (stard) 2019.1.21
                                /*
								//Phase 2-1 of Beam Training
								int candidate_phase2[4][3] = { { 1,2,4 },{ 4,5,6 },{ 7,8,9 },{ 9,10,1 } }, count_rx_phase2_part1[3] = { 0 }, record_count_rx_phase2_part1[3] = { 0 };
								flag_snr = 0;
								beam_snr_ptr = (int *)beam_snr_phase2_part1; //pointer to beam_snr_phase2_part1
								int tmp2_p1 = 0, rx_beam_phase2_part1 = 0, record_rx_id_phase2_part1 = 0;
								for (int i = 0; i < 3; i++) {
									for (int j = 0; j < 3; j++) {
										ML_Init(); //Initial the device and SDK first.
										ML_SetSpeed(mcs_record);
										ML_SetRxSector(candidate_phase2[rx_beam_id_phase1[0]][j]);
										fs << "RX_Beam ID:" << candidate_phase2[rx_beam_id_phase1[0]][j] << std::endl;
										Layer = 2;
										BeamID = candidate_phase2[rx_beam_id_phase1[0]][j];
										pthread_create(&show_bitrate, NULL, ShowBitrate, NULL);
										SpeedRx(beam_snr_ptr, flag_snr, Layer, BeamID);
										RxGetRF();
										flag_snr = 1;
										ML_Close(); //Close the device and leave SDK
									}
								}
								//Print Results	and choose the highest snr								
								for (int i = 0; i < 3; i++) {
									for (int j = 0; j < 3; j++) {
										fs << "beam_SNR_phase2_part1[" << i << "][" << j << "]: " << beam_snr_phase2_part1[i][j] << std::endl;
										if (beam_snr_phase2_part1[i][j] > tmp2_p1) {
											tmp2_p1 = beam_snr_phase2_part1[i][j];
										}
									}
								}
								
								//Select optimal beam sector
								for (int i = 0; i < 3; i++) {
									for (int j = 0; j < 3; j++) {
										if (beam_snr_phase2_part1[i][j] == tmp2_p1) {
											count_rx_phase2_part1[j]++;
											record_count_rx_phase2_part1[j]++;
										}
									}
								}
								for (int i = 0; i < 3; i++) fs << "count_rx_phase2[" << i << "]:" << count_rx_phase2_part1[i] << std::endl;
								std::sort(record_count_rx_phase2_part1, record_count_rx_phase2_part1 + 3);
								std::reverse(record_count_rx_phase2_part1, record_count_rx_phase2_part1 + 3);
								for (int i = 0; i < 3; i++) fs << "record_count_rx_phase1(sort)[" << i << "]:" << record_count_rx_phase2_part1[i] << std::endl;
								for (int num = 0; num < 1; num++) { 
									for (int k = 0; k < 3; k++) {
										if (count_rx_phase2_part1[k] == record_count_rx_phase2_part1[num]) {
											rx_beam_phase2_part1 = candidate_phase2[rx_beam_id_phase1[0]][k];
											record_rx_id_phase2_part1 = k;
										}
									}
								}

								//Phase 2-2 of Beam Training
								flag_snr = 0;
								beam_snr_ptr = (int *)beam_snr_phase2_part2; //pointer to beam_snr_phase2_part2
								int count_rx_phase2_part2[3] = { 0 }, record_count_rx_phase2_part2[3] = { 0 };
								int tmp2_p2 = 0, rx_beam_phase2_part2 = 0, record_rx_id_phase2_part2 = 0;
								for (int i = 0; i < 3; i++) {
									for (int j = 0; j < 3; j++) {
										ML_Init(); //Initial the device and SDK first.
										ML_SetSpeed(mcs_record);
										ML_SetRxSector(candidate_phase2[rx_beam_id_phase1[1]][j]);
										fs << "RX_Beam ID:" << candidate_phase2[rx_beam_id_phase1[1]][j] << std::endl;
										Layer = 2;
										BeamID = candidate_phase2[rx_beam_id_phase1[1]][j];
										pthread_create(&show_bitrate, NULL, ShowBitrate, NULL);
										SpeedRx(beam_snr_ptr, flag_snr, Layer, BeamID);
										RxGetRF();
										flag_snr = 1;
										ML_Close(); //Close the device and leave SDK
									}
								}
								//Print Results	and choose the highest snr								
								for (int i = 0; i < 3; i++) {
									for (int j = 0; j < 3; j++) {
										fs << "beam_SNR_phase2_part2[" << i << "][" << j << "]: " << beam_snr_phase2_part2[i][j] << std::endl;
										if (beam_snr_phase2_part2[i][j] > tmp2_p2) {
											tmp2_p2 = beam_snr_phase2_part2[i][j];
										}
									}
								}

								//Select optimal beam sector
								for (int i = 0; i < 3; i++) {
									for (int j = 0; j < 3; j++) {
										if (beam_snr_phase2_part2[i][j] == tmp2_p2) {
											count_rx_phase2_part2[j]++;
											record_count_rx_phase2_part2[j]++;
										}
									}
								}
								for (int i = 0; i < 3; i++) fs << "count_rx_phase2_part2[" << i << "]:" << count_rx_phase2_part2[i] << std::endl;
								std::sort(record_count_rx_phase2_part2, record_count_rx_phase2_part2 + 3);
								std::reverse(record_count_rx_phase2_part2, record_count_rx_phase2_part2 + 3);
								for (int i = 0; i < 3; i++) fs << "record_count_rx_phase2_part2(sort)[" << i << "]:" << record_count_rx_phase2_part2[i] << std::endl;

								for (int num = 0; num < 1; num++) {
									for (int k = 0; k < 3; k++) {
										if (count_rx_phase2_part2[k] == record_count_rx_phase2_part2[num]) {
											rx_beam_phase2_part2 = candidate_phase2[rx_beam_id_phase1[1]][k];
											record_rx_id_phase2_part2 = k;
										}
									}
								}
								fs << "End Beam Training Phase 2!!" << std::endl;
								fs << "Rx_beam_phase2: Sector " << rx_beam_phase2_part1 << " and Sector " << rx_beam_phase2_part2 << std::endl;

								//Phase 3-1 of Beam Training
								int candidate_phase3[2][4][3] = { { { 1,2,4 },{ 4,4,5 },{ 5,5,8 },{ 9,1,1 } },{ { 2,4,5 },{ 5,5,6 },{ 7,8,9 },{ 10,9,10 } } };
								flag_snr = 0;
								beam_snr_ptr = (int *)beam_snr_phase3_part1; //pointer to beam_snr_phase3_part1
								int tmp3_p1 = 0;
								for (int i = 0; i < 2; i++) {
									for (int j = 0; j < 2; j++) {
										ML_Init(); //Initial the device and SDK first.
										ML_SetSpeed(mcs_record);
										ML_SetRxSector(candidate_phase3[j][rx_beam_id_phase1[0]][record_rx_id_phase2_part1]);
										fs << "RX_Beam ID:" << candidate_phase3[j][rx_beam_id_phase1[0]][record_rx_id_phase2_part1] << std::endl;
										Layer = 3;
										BeamID = candidate_phase3[j][rx_beam_id_phase1[0]][record_rx_id_phase2_part1];
										pthread_create(&show_bitrate, NULL, ShowBitrate, NULL);
										SpeedRx(beam_snr_ptr, flag_snr, Layer, BeamID);
										RxGetRF();
										flag_snr = 1;
										ML_Close(); //Close the device and leave SDK
									}
								}
								//Print Results										
								for (int i = 0; i < 2; i++) {
									for (int j = 0; j < 2; j++) {
										fs << "beam_SNR_phase3_part1[" << i << "][" << j << "]: " << beam_snr_phase3_part1[i][j] << std::endl;
										if (beam_snr_phase3_part1[i][j]>tmp3_p1) {
											tmp3_p1 = beam_snr_phase3_part1[i][j];
										}
									}
								}

								//Phase 3-2 of Beam Training
								flag_snr = 0;
								beam_snr_ptr = (int *)beam_snr_phase3_part2; //pointer to beam_snr_phase3_part2
								int tmp3_p2 = 0;
								for (int i = 0; i < 2; i++) {
									for (int j = 0; j < 2; j++) {
										ML_Init(); //Initial the device and SDK first.
										ML_SetSpeed(mcs_record);
										ML_SetRxSector(candidate_phase3[j][rx_beam_id_phase1[1]][record_rx_id_phase2_part2]);
										fs << "RX_Beam ID:" << candidate_phase3[j][rx_beam_id_phase1[1]][record_rx_id_phase2_part2] << std::endl;
										Layer = 3;
										BeamID = candidate_phase3[j][rx_beam_id_phase1[1]][record_rx_id_phase2_part2];
										pthread_create(&show_bitrate, NULL, ShowBitrate, NULL);
										SpeedRx(beam_snr_ptr, flag_snr, Layer, BeamID);
										RxGetRF();
										flag_snr = 1;
										ML_Close(); //Close the device and leave SDK
									}
								}
								//Print Results										
								for (int i = 0; i < 2; i++) {
									for (int j = 0; j < 2; j++) {
										fs << "beam_SNR[" << i << "][" << j << "]: " << beam_snr_phase3_part2[i][j] << std::endl;
										if (beam_snr_phase3_part2[i][j]>tmp3_p2) {
											tmp3_p2 = beam_snr_phase3_part2[i][j];
										}
									}
								}
								*/
                                //the code will be commend here (end) 2019.1.21
                                /*
								int rx_beam_phase3 = 0, count_rx_phase3[2] = { 0 }, record_count_rx_phase3[2] = { 0 };
								if (tmp3_p1>tmp3_p2) {
									for (int i = 0; i < 2; i++) {
										for (int j = 0; j < 2; j++) {
											if (beam_snr_phase3_part1[i][j] == tmp3_p1) {
												count_rx_phase3[j]++;
												record_count_rx_phase3[j]++;
											}
										}
									}
									for (int i = 0; i < 2; i++) fs << "count_rx_phase3_part1[" << i << "]:" << count_rx_phase3[i] << std::endl;
									std::sort(record_count_rx_phase3, record_count_rx_phase3 + 2);
									std::reverse(record_count_rx_phase3, record_count_rx_phase3 + 2);
									for (int i = 0; i < 2; i++) fs << "record_count_rx_phase3_part1(sort)[" << i << "]:" << record_count_rx_phase3[i] << std::endl;

									for (int num = 0; num < 1; num++) {
										for (int k = 0; k < 2; k++) {
											if (count_rx_phase3[k] == record_count_rx_phase3[num]) {
												rx_beam_phase3 = candidate_phase3[k][rx_beam_id_phase1[0]][record_rx_id_phase2_part1];
												//record_rx_id_phase3 = k;
											}
										}
									}
								}
								else {
									for (int i = 0; i < 2; i++) {
										for (int j = 0; j < 2; j++) {
											if (beam_snr_phase3_part2[i][j] == tmp3_p2) {
												count_rx_phase3[j]++;
												record_count_rx_phase3[j]++;
											}
										}
									}
									for (int i = 0; i < 2; i++) fs << "count_rx_phase3_part2[" << i << "]:" << count_rx_phase3[i] << std::endl;
									std::sort(record_count_rx_phase3, record_count_rx_phase3 + 2);
									std::reverse(record_count_rx_phase3, record_count_rx_phase3 + 2);
									for (int i = 0; i < 2; i++) fs << "record_count_rx_phase3_part2(sort)[" << i << "]:" << record_count_rx_phase3[i] << std::endl;

									for (int num = 0; num < 1; num++) {
										for (int k = 0; k < 2; k++) {
											if (count_rx_phase3[k] == record_count_rx_phase3[num]) {
												rx_beam_phase3 = candidate_phase3[k][rx_beam_id_phase1[1]][record_rx_id_phase2_part2];
												//record_rx_id_phase3 = k;
											}
										}
									}
								}
                                */
								
								printf("==========search result(tx)========== \n");
								printf("exhaustive_search_result_tx_r(suppose): %d \n",exhaustive_search_result_tx_r);
								printf("exhaustive_search_result_rx_r         : %d \n",exhaustive_search_result_rx_r);
								printf("===================================== \n");
								
								fprintf(stdout, "\033[27;3H End Beam Training \n");
								fprintf(stdout, "\033[28;3H Best Rx_beam: %d\n\n", exhaustive_search_result_rx_r);
								fs << "End Beam Training Phase 3!!" << std::endl;
								fs << "Rx_beam_final: Sector " << exhaustive_search_result_rx_r << std::endl;

								fs.close();

							}
							else {
								fprintf(stdout, "Fail, Please select funcion");
							}
						}
					}
					else {
						fprintf(stdout, "Cannot Start, Please set rule\n");
					}
				}
				else {
					fprintf(stdout, "Please close function\n");
				}
				break;
#endif
			case 0:
				if (running) {
					s_index = false;
					pthread_join(thread, NULL);
					pthread_join(show_bitrate, NULL);
#ifdef RF_STATUS
					pthread_join(getRFstatus, NULL);
#endif
					running = false;
				}
				break;
			default:
				fprintf(stdout, "error\n");
				continue;
			}
		} while (menu != 0);
		break;
	}

	printf("\033[2J");
#endif
	return 0;
}

void* ShowBitrate(void *ptr) {
	//long bitrate = 0;

	while (s_index) {
		//bitrate = ((bits * 8) / (1024 *1024));
		//fprintf(stdout,"\033[8;0H  %ld Mbp/s\n", bitrate);
		fflush(stdout);
		bits = 0;
		sleep(1);
	}
	return ((void *)0);
}

#if 1
void SpeedTx(int BeamID) {
	unsigned char* buf = (unsigned char*)malloc(BUFSIZE * 1 * sizeof(char));
	time_t  timer = time(NULL); // CPU time
	memset(buf, 0, BUFSIZE * 1);
	//for (int i = 0; i < 4; i++) buf[i] = i + 1; //fill the Specific Char. to the header of buffer
	buf[4] = BeamID; //fill the Tx_SectorID	          different beam different code (first should be specific)
	int length = BUFSIZE * 1, status;
	int count = 1;
	
        sleep(1);
	
	while (count <= (pkts)) {                             // need to be fixed (synchronize)
		//fixed(2019.1.7)==start==
		for (int i = 0; i < 4; i++) buf[i] = i + 1 + (count-1)*10; 
		//fixed(2019.1.7)== end ==
		buf[5] = pkt_no; //fill the Pkt number
		
		status = ML_Transfer(buf, length);
		if (status > 0) {
			bits += length;
		}
		// for debug 2019/1/9
		
		//
		count++;
		//pkt_no++; (1.20)
                if (pkt_no == 90) {
                   pkt_no = 0;
                }
                else pkt_no ++;
                


		
		printf("============================== \n");
		printf("pkt_no: %d \n", pkt_no);
		printf("Tx count: %d /n", count);
		printf("Tx status: %d \n", status);
		printf("Tx Beam ID: %d \n",BeamID);
		printf("ctime is %s\n", ctime(&timer)); // CPU time
		printf("============================== \n");
		
	}
	free(buf);
}

int SpeedRx(int *ptr, int flag_snr, int Layer, int BeamID) {
	uint8_t* buf = (uint8_t*)malloc(BUFSIZE * 1);
	memset(buf, 0, BUFSIZE * 1);
	int status, length;
	time_t  timer = time(NULL); // CPU time
	int count_snr_available = 0, total_snr = 0, average_snr = 0;
	static int *beam_snr_ptr = (int *)ptr;
	if (flag_snr == 0) beam_snr_ptr = (int *)ptr;
	
	std::fstream fs;
	fs.open("rx_snr_record.txt", std::fstream::out | std::fstream::app);
	int count = 0, retry_no = 0;
        
        sleep(1);
	int flag_receive;
	int RX_beamsector_next[10];
	for(int j=0;j<10;j++)
		RX_beamsector_next[j]=0;
	while(count < pkts){                //need to be fixed 
		flag_receive = 0;
		length = BUFSIZE * 1;

                int check_to_break_while = 0;  //2019.2.21
        if(buf[5]<91)
        {
			int tmp=0;
			tmp=buf[5]/10+1;
			RX_beamsector_next[tmp-1]++;
		}
		if (rx_rfstatus) {
			ML_SendRFStatusReq();
			rx_rfstatus = false;
		}
		// for debug 2019/1/9
		
		// for debug
		// for RX_start
		if (RX_start == 0) {
			while (1) {
				if (ML_Receiver(buf, &length)) {
					RX_start = 1;
					break;
				}
			}
		}
		else {
			status = ML_Receiver(buf, &length);
		}
		// 
		//status = ML_Receiver(buf, &length);
	                                               //need to be fixed (package was changed)
		//if (buf[0] == 1 && buf[1] == 2 && buf[2] == 3 && buf[3] == 4) {
		//	fs << "Make sure the buffer, correct" << std::endl;
		//	flag_receive = 1;
		//	count++;
		//	fs << "count:" << (int)count << std::endl;
		//}
		//else retry_no++;
		//fixed(2019.1.7)==start== 
		if (buf[0] == 1 && buf[1] == 2 && buf[2] == 3 && buf[3] == 4 && status) {
			fs << "Make sure the buffer, correct" << std::endl;
		    flag_receive = 1;
		    //count=1;
		    count++;
			//retry_no = 0;
		    fs << "count:" << (int)count << std::endl;
		}
		else if (buf[0] == 11 && buf[1] == 12 && buf[2] == 13 && buf[3] == 14 && status) {
			fs << "Make sure the buffer, correct" << std::endl;
		    flag_receive = 1;
		    //count=2;
		    if (count<2){
		    	count = 2;
			}
			else count++;
			//retry_no = 0;
		    fs << "count:" << (int)count << std::endl;
		}
		else if (buf[0] == 21 && buf[1] == 22 && buf[2] == 23 && buf[3] == 24 && status) {
			fs << "Make sure the buffer, correct" << std::endl;
		    flag_receive = 1;
		    //count=3;
		    if (count<3){
		    	count = 3;
			}
			else count++;
			//retry_no = 0;
		    fs << "count:" << (int)count << std::endl;
		}
		else if (buf[0] == 31 && buf[1] == 32 && buf[2] == 33 && buf[3] == 34 && status) {
			fs << "Make sure the buffer, correct" << std::endl;
		    flag_receive = 1;
		    //count=4;
		    if (count<4){
		    	count = 4;
			}
			else count++;
			
		    //retry_no = 0;
		    fs << "count:" << (int)count << std::endl;
		}
		else if (buf[0] == 41 && buf[1] == 42 && buf[2] == 43 && buf[3] == 44 && status) {
			fs << "Make sure the buffer, correct" << std::endl;
		    flag_receive = 1;
		    //count=5;
		    if (count<5){
		    	count = 5;
			}
			else count++;
		    //retry_no = 0;
		    fs << "count:" << (int)count << std::endl;
		}
		else if (buf[0] == 51 && buf[1] == 52 && buf[2] == 53 && buf[3] == 54 && status) {
			fs << "Make sure the buffer, correct" << std::endl;
		    flag_receive = 1;
		    //count=6;
		    if (count<6){
		    	count = 6;
			}
			else count++;
		    //retry_no = 0;
		    fs << "count:" << (int)count << std::endl;
		}
		else if (buf[0] == 61 && buf[1] == 62 && buf[2] == 63 && buf[3] == 64 && status) {
			fs << "Make sure the buffer, correct" << std::endl;
		    flag_receive = 1;
		    //count=7;
		    if (count<7){
		    	count = 7;
			}
			else count++;
			//retry_no = 0;
		    fs << "count:" << (int)count << std::endl;
		}
		else if (buf[0] == 71 && buf[1] == 72 && buf[2] == 73 && buf[3] == 74 && status) {
			fs << "Make sure the buffer, correct" << std::endl;
		    flag_receive = 1;
		    //count=8;
		    if (count<8){
		    	count = 8;
			}
			else count++;
		    retry_no = 0;
		    fs << "count:" << (int)count << std::endl;
		}
		else if (buf[0] == 81 && buf[1] == 82 && buf[2] == 83 && buf[3] == 84 && status) {
			fs << "Make sure the buffer, correct" << std::endl;
		    flag_receive = 1;
		    //count=9;
		    if (count<9){
		    	count = 9;
			}
			else count++;
		    //retry_no = 0;
		    fs << "count:" << (int)count << std::endl;
		}
		else if (buf[0] == 91 && buf[1] == 92 && buf[2] == 93 && buf[3] == 94 && status) {
			fs << "Make sure the buffer, correct" << std::endl;
		    flag_receive = 1;
		    count=10;
		    //retry_no = 0;
		    fs << "count:" << (int)count << std::endl;
		}
		else retry_no++;
		//fixed(2019.1.7)== end ==
		fs << "RXBeamID = " << (int)BeamID << std::endl;
		fs << "PacketNo = " << (int)buf[5] << std::endl;
		fs << "TXBeamID = " << (int)buf[4] << std::endl;
		if(retry_no == retry_threshold) break;
                
                //2019.1.21
                //if(flag_receive == 1){
                //    check_to_break_while = (int) ( (buf[5]-1) % 10 ) ;
                //    if(count > check_to_break_while) break;
                //}//2019.1.21
		
		/*printf("============================== \n");
		printf("buf[4]: %d \n",buf[4]);
		printf("buf[5]: %d \n",buf[5]);
		printf("Rx Beam ID: %d \n",BeamID);
		printf("Rx cound:%d \n",count);
		printf("Rx retry:%d \n",retry);
		printf("Rx status:%d \n",status);
		printf("flag: %d \n", flag_receive);
		printf("============================== \n");*/

		if (flag_receive ==1 ) {           //need to be fixed (delete some of the code)
			bits += BUFSIZE * 1;
			for (int i = 0; i < 1; i++)
			{
				if (ML_DecodeRFStatusPacket(buf + (i*BUFSIZE), &ML_RF_Record))
				{
				//	readRFPackets(ML_RF_Record, Layer, BeamID);
					if (ML_RF_Record.PHY_RX_SNR != 0) count_snr_available++;
					total_snr += ML_RF_Record.PHY_RX_SNR;
					fs << "PHY_RX_SNR_ptr:" << (int)ML_RF_Record.PHY_RX_SNR << std::endl;
				}
			}
			// 2019/1/9 print data
			printf("============================== \n");
			printf("buf[4]: %d \n",buf[4]);
			printf("buf[5]: %d \n",buf[5]);
			printf("Rx Beam ID: %d \n",BeamID);
			printf("Rx cound:%d \n",count);
			printf("Rx retry_no:%d \n",retry_no);
			printf("Rx status:%d \n",status);
			printf("flag: %d \n", flag_receive);
			printf("ctime is %s\n", ctime(&timer)); // CUP time
			printf("============================== \n");
                        fs << "==============================" << std::endl;
                        fs << "buf[4]: " << (int)buf[4] << std::endl;
                        fs << "buf[5]: " << (int)buf[5] << std::endl;
                        fs << "Rx Beam ID: " << (int)BeamID << std::endl;
                        fs << "ctime is " << ctime(&timer) << std::endl;
			//
		}
		else {
			//fprintf(stdout,"Rx fail return:%d", status);
			fs << "Rx fail" << std::endl;
			ML_RF_Record.PHY_RX_SNR = 0;
			fs << "PHY_RX_SNR_ptr:" << (int)ML_RF_Record.PHY_RX_SNR << std::endl;
		}
	}
	//Notice
	if (count_snr_available > pkts) count_snr_available = pkts;
	if (total_snr != 0 && count_snr_available != 0)	average_snr = total_snr / count_snr_available;
	fs << "average_snr:" << average_snr << std::endl;
	fs << "--------------------------" << std::endl;
	*beam_snr_ptr = average_snr;
	beam_snr_ptr++; //ONLY record the last packets in the same beam trail
	fs.close();
	free(buf);
	// (1.21)
	int sort=0;
	for(int j=0;j<9;j++)
	{
		if(RX_beansector_next[j]<RX_beansector_next[j+1])
			sort=j;
	}
	return sort+1;
	/*if (flag_receive == 1)
		return buf[5];
	else 
		return -1;*/
}
#endif

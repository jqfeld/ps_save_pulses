#include <H5Ipublic.h>
#include <H5Ppublic.h>
#include <H5Tpublic.h>
#include <hdf5.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include <libps5000a/PicoStatus.h>
#include <libps5000a/ps5000aApi.h>


const int64_t version = 2;
void color_bold() { printf("\033[1m"); }

void color_red() { printf("\033[0;31m"); }

void color_green() { printf("\033[0;32m"); }

void color_reset() { printf("\033[0m"); }

void expect_ok(PICO_STATUS ret) {
  if (ret != PICO_OK) {
    color_red();
    printf("\nError: 0x%08X\n", ret);
    color_reset();
    exit(EXIT_FAILURE);
  }
}

void print_trigger_info(PS5000A_TRIGGER_INFO trigger_info) {
  printf("{STATUS: 0x%08X, idx: %d, trigger_time: %ld, time_stamp: %lu}",
         trigger_info.status, trigger_info.segmentIndex,
         trigger_info.triggerTime, trigger_info.timeStampCounter);
}

/*PS5000A_10MV,*/
/*PS5000A_20MV,*/
/*PS5000A_50MV,*/
/*PS5000A_100MV,*/
/*PS5000A_200MV,*/
/*PS5000A_500MV,*/
/*PS5000A_1V,*/
/*PS5000A_2V,*/
/*PS5000A_5V,*/
/*PS5000A_10V,*/
/*PS5000A_20V,*/
/*PS5000A_50V,*/
double range_to_double(enum enPS5000ARange range) {
  switch (range) {
  case PS5000A_500MV:
    return .5;
    break;
  case PS5000A_1V:
    return 1.0;
    break;
  case PS5000A_2V:
    return 2.0;
    break;
  case PS5000A_5V:
    return 5.0;
    break;
  case PS5000A_10V:
    return 10.0;
    break;
  default:
    color_red();
    printf("Error: range_to_double not implemented for this range.");
    color_reset();
    exit(EXIT_FAILURE);
    return 0.0;
  }
}

int main(int argc, char *argv[]) {

  PICO_STATUS ret;
  int16_t handle;

  if (argc < 2) {
    printf("At least save file name necessary:\n\n");
    printf("\t%s file [time_interval_ns scan_time_s discharge_period_s range_A "
           "range_B]\n",
           argv[0]);
    exit(EXIT_FAILURE);
  }

  /*-------------------------------------------------------------------------*/
  /*                               Arguments                                 */
  /*-------------------------------------------------------------------------*/
  float time_interval_ns = 50.0;
  double scan_time_s = 30.;
  double discharge_period_s = 0.001;
  double pretrigger_time_s = 0e-6;
  enum enPS5000ARange range_A = PS5000A_5V;
  enum enPS5000ARange range_B = PS5000A_2V;
  char *file = argv[1];
  /*char file[] = "signal.h5";*/

  if (argc > 2) {
    sscanf(argv[2], "%f", &time_interval_ns);
  }
  if (argc > 3) {
    sscanf(argv[3], "%lf", &scan_time_s);
  }
  if (argc > 4) {
    sscanf(argv[4], "%lf", &discharge_period_s);
  }
  if (argc > 5) {
    sscanf(argv[5], "%d", &range_A);
  }
  if (argc > 6) {
    sscanf(argv[6], "%d", &range_B);
  }

  /*-------------------------------------------------------------------------*/
  /*                               Parameters                                */
  /*-------------------------------------------------------------------------*/
  int32_t timebase = 0;
  int32_t num_samples = 100;
  uint32_t num_captures = floor(scan_time_s / discharge_period_s);

  ret = ps5000aOpenUnit(&handle, NULL, PS5000A_DR_15BIT);

  if (ret != PICO_OK) {
    printf("Error while opening device: %d\n", ret);
    return ret;
  }
  printf("Connected to PicoScope. Handle: %d\n", handle);

  printf("Setting up channels... ");
  ret =
      ps5000aSetChannel(handle, PS5000A_CHANNEL_A, 1, PS5000A_DC, range_A, 0.0);
  if (ret != PICO_OK) {
    printf("\nError while setting up Ch A: %d\n", ret);
    return ret;
  }
  ret =
      ps5000aSetChannel(handle, PS5000A_CHANNEL_B, 1, PS5000A_DC, range_B, 0.0);
  if (ret != PICO_OK) {
    printf("\nError while setting up Ch B: %d\n", ret);
    return ret;
  }
  color_green();
  printf("done!\n");
  color_reset();

  printf("Setting memory segments to %d...", num_captures);
  int32_t max_samples_segment = 0;
  ret = ps5000aMemorySegments(handle, num_captures, &max_samples_segment);
  expect_ok(ret);
  ret = ps5000aSetNoOfCaptures(handle, num_captures);
  expect_ok(ret);
  printf("done! Max. samples per segment: %d\n", max_samples_segment);

  printf("Selecting time base to get dt<=%.2f ns...", time_interval_ns);
  float requested_time_interval_ns = time_interval_ns;
  int32_t max_samples = 0;
  float next_time_interval_ns = 0.0;
  for (int i = 0; next_time_interval_ns <= requested_time_interval_ns; i++) {
    /*printf("timebase: %d\n", timebase);*/
    time_interval_ns = next_time_interval_ns;
    ret = ps5000aGetTimebase2(handle, i + 1, num_samples,
                              &next_time_interval_ns, &max_samples, 0);
    if (ret == PICO_INVALID_TIMEBASE) {
      /*printf("\nInvalid time base: %d\n", i+1);*/
      continue;
    } else if (ret != PICO_OK) {
      printf("\nError while getting timebase: 0x%08X\n", ret);
      return ret;
    }

    timebase = i;
  }
  printf("Found timebase \033[1m%d\033[0m giving \033[1m%.2f ns\033[0m (%d "
         "samples)\n",
         timebase, time_interval_ns, max_samples);

  printf("Get max. samples in discharge period...");
  int32_t max_samples_per_period =
      floor((discharge_period_s - 100e-6) / (time_interval_ns * 1e-9));
  printf("done! Got max. %d samples.\n", max_samples_per_period);

  num_samples = (max_samples_per_period < max_samples) ? max_samples_per_period
                                                       : max_samples;
  printf("Finally using %d samples per capture.\n", num_samples);

  printf("Setting trigger...");
  ret = ps5000aSetSimpleTrigger(handle, 1, PS5000A_EXTERNAL, 10000,
                                PS5000A_RISING, 0, 0);
  expect_ok(ret);
  printf("done!\n");

  printf("Setup signal generator...");
  double siggen_freq =
      1 / (scan_time_s * 1.1) > 0.03 ? 1 / (scan_time_s * 1.1) : 0.03;
  ret = ps5000aSetSigGenBuiltInV2(handle,
                                  2000000, // in microvolt
                                  4000000, // in microvolt
                                  PS5000A_SQUARE, siggen_freq, siggen_freq, 0,
                                  0, 0, 0, 1, 0, PS5000A_SIGGEN_RISING,
                                  PS5000A_SIGGEN_SOFT_TRIG, 10000);
  expect_ok(ret);

  printf("done!\n");

  printf("Pulse signal generator...");

  ret = ps5000aSigGenSoftwareControl(handle, 1);
  expect_ok(ret);
  printf("done!\n");

  printf("Starting capture...\n");
  int32_t pretrigger_samples =
      floor(pretrigger_time_s / (time_interval_ns * 1e-9));
  ret = ps5000aRunBlock(handle, pretrigger_samples,
                        num_samples - pretrigger_samples, timebase, NULL, 0,
                        NULL, NULL);
  expect_ok(ret);

  int16_t is_ready = 0;
  uint32_t cur_captures = 0;
  while (!is_ready) {
    ps5000aIsReady(handle, &is_ready);
    ps5000aGetNoOfCaptures(handle, &cur_captures);
    color_bold();
    printf("\r%5d / %5d", cur_captures, num_captures);
    color_reset();
    fflush(stdout);
    sleep(1);
  }
  color_green();
  printf("...done!\n");
  color_reset();

  printf("Allocate data %d x %d buffers...", num_captures, num_samples);
  int16_t *buffer_channel_A =
      calloc(num_samples * num_captures, sizeof(int16_t));
  int16_t *buffer_channel_B =
      calloc(num_samples * num_captures, sizeof(int16_t));
  for (int i = 0; i < num_captures; i++) {
    ret = ps5000aSetDataBuffer(handle, PS5000A_CHANNEL_A,
                               buffer_channel_A + num_samples * i, num_samples,
                               i, PS5000A_RATIO_MODE_NONE);
    expect_ok(ret);
    ret = ps5000aSetDataBuffer(handle, PS5000A_CHANNEL_B,
                               buffer_channel_B + num_samples * i, num_samples,
                               i, PS5000A_RATIO_MODE_NONE);
    expect_ok(ret);
  }
  printf("done!\n");

  printf("Download values form scope...\n");

  /* Bulk transfer of captures */

  uint32_t request_samples = num_samples;
  int16_t overflow[num_captures];
  ret = ps5000aGetValuesBulk(handle, &request_samples, 0, num_captures - 1, 1,
                             PS5000A_RATIO_MODE_NONE, overflow);

  expect_ok(ret);

  if (request_samples != num_samples) {
    color_red();
    printf(
        "\rWarning: Number of retrieved samples != num_samples! %d vs. %d.\n",
        request_samples, num_samples);
    color_reset();
  }

  if (overflow) {
    color_red();
    printf("Overflow: %2b", overflow);
    color_reset();
  }

  color_green();
  printf("...done!\n");
  color_reset();

  printf("Retrieve trigger info...");
  PS5000A_TRIGGER_INFO trigger_info[num_captures];
  ret = ps5000aGetTriggerInfoBulk(handle, trigger_info, 0, num_captures - 1);
  expect_ok(ret);
  printf("done!\n");

  /*printf("First 10 trigger infos:\n");*/
  /*for (int i = 0; i < 10; i++) {*/
  /*  printf("\t");*/
  /*  print_trigger_info(trigger_info[i]);*/
  /*  printf("\n");*/
  /*}*/
  /*printf("Last 10 trigger infos:\n");*/
  /*for (int i = num_captures - 1; i > num_captures - 11; i--) {*/
  /*  printf("\t");*/
  /*  print_trigger_info(trigger_info[i]);*/
  /*  printf("\n");*/
  /*}*/

  /*-------------------------------------------------------------------------*/
  /*                                  Saving                                 */
  /*-------------------------------------------------------------------------*/

  /*printf("Last 10 buffer_A:\n");*/
  /*for (int i = 0; i < 10; i++) {*/
  /*  printf("\t");*/
  /*  printf("%d", buffer_channel_A[i]);*/
  /*  printf("\n");*/
  /*}*/

  int16_t max_ADC_value;
  ret = ps5000aMaximumValue(handle, &max_ADC_value);
  expect_ok(ret);

  /*for (int i = 0; i < num_captures * num_samples; i++) {*/
  /*  voltage_A[i] =*/
  /*      (range_to_double(range_A) * buffer_channel_A[i]) / max_ADC_value;*/
  /*  voltage_B[i] =*/
  /*      (range_to_double(range_B) * buffer_channel_B[i]) / max_ADC_value;*/
  /*}*/

  /*printf("Last 10 voltage_A:\n");*/
  /*for (int i = 0; i < 10; i++) {*/
  /*  printf("\t");*/
  /*  printf("%f", voltage_A[i]);*/
  /*  printf("\n");*/
  /*}*/

  printf("Saving data to file %s...", file);
  hid_t file_id, dataspace_id, scalarspace_id, voltage_A_id, voltage_B_id,
      parameters_id;
  herr_t status;
  hsize_t dims[2];
  dims[0] = num_captures;
  dims[1] = num_samples;
  hsize_t pdims = 1;

  file_id = H5Fcreate(file, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);

  dataspace_id = H5Screate_simple(2, dims, NULL);
  scalarspace_id = H5Screate_simple(0, NULL, NULL);

  voltage_A_id = H5Dcreate2(file_id, "/channel_A", H5T_INTEL_I16, dataspace_id,
                            H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  voltage_B_id = H5Dcreate2(file_id, "/channel_B", H5T_INTEL_I16, dataspace_id,
                            H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  status = H5Dwrite(voltage_A_id, H5T_NATIVE_INT16, H5S_ALL, H5S_ALL,
                    H5P_DEFAULT, buffer_channel_A);
  status = H5Dwrite(voltage_B_id, H5T_NATIVE_INT16, H5S_ALL, H5S_ALL,
                    H5P_DEFAULT, buffer_channel_B);
  double parameter_buffer[] = {time_interval_ns,
                               range_to_double(range_A) / max_ADC_value,
                               range_to_double(range_B) / max_ADC_value};

  hid_t id;
  // save file data version
  id = H5Dcreate2(file_id, "/version", H5T_INTEL_I16, scalarspace_id,
                 H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  status = H5Dwrite(id, H5T_NATIVE_INT16, H5S_ALL, H5S_ALL,
                    H5P_DEFAULT, &version);
  status = H5Dclose(id);
  
  // save time_interval_ns 
  id = H5Dcreate2(file_id, "/time_interval_ns", H5T_INTEL_F32, scalarspace_id,
                 H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  status = H5Dwrite(id, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL,
                    H5P_DEFAULT, &time_interval_ns);
  status = H5Dclose(id);

  // save scale_factor_A 
  double scale_factor_A = range_to_double(range_A) / max_ADC_value;
  id = H5Dcreate2(file_id, "/scale_factor_A", H5T_INTEL_F64, scalarspace_id,
                 H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  status = H5Dwrite(id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL,
                    H5P_DEFAULT, &scale_factor_A);
  status = H5Dclose(id);
  // save scale_factor_B 
  double scale_factor_B = range_to_double(range_A) / max_ADC_value;
  id = H5Dcreate2(file_id, "/scale_factor_B", H5T_INTEL_F64, scalarspace_id,
                 H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  status = H5Dwrite(id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL,
                    H5P_DEFAULT, &scale_factor_B);
  status = H5Dclose(id);

  status = H5Dclose(voltage_A_id);
  status = H5Dclose(voltage_B_id);
  status = H5Sclose(dataspace_id);
  status = H5Fclose(file_id);

  printf("done!\n");

  exit(EXIT_SUCCESS);
}

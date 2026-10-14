// Copyright 2025-2026 Stefan Zellmann
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <float.h>
#include <stdio.h>
#include <math.h>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>
#include <netcdf.h>
#ifdef WITH_UMESH
# include "umesh/umesh.h"
#endif

struct {
  std::string inputFile;
  bool outputBinary{true};
  bool outputText{true};
  bool outputLegacyVTK{true};
  std::string varName{"QC"};
  int imgSize[2]{1000,200};
  int roi[4]{-1,-1,-1,-1};
} g_appState;

static void printHelp() {
}

static size_t readDimLength(int ncid, std::string name) {
  int retval, dimid;

  if ((retval = nc_inq_dimid(ncid, name.c_str(), &dimid)) != NC_NOERR) {
    fprintf(stderr, "dim %s not found: %s\n", name.c_str(), nc_strerror(retval));
    return ~0ull;
  }

  size_t result;
  if ((retval = nc_inq_dimlen(ncid, dimid, &result)) != NC_NOERR) {
    fprintf(stderr, "variable %s found but size mismatch\n", name.c_str());
    return ~0ull;
  }

  return result;
}

static std::vector<int> readIntVar(int ncid, std::string name, size_t len) {
  int retval, varid;
  if ((retval = nc_inq_varid(ncid, name.c_str(), &varid)) != NC_NOERR) {
    fprintf(stderr, "variable %s not found\n", name.c_str());
    return {};
  }

  std::vector<int> result(len);

  if ((retval = nc_get_var_int(ncid, varid, result.data())) != NC_NOERR) {
    fprintf(stderr, "cannot read from variable %s\n", name.c_str());
    return {};
  }

  if (result.size() != len) {
    fprintf(stderr, "variable %s found but size mismatch\n", name.c_str());
    return {};
  }

  return result;
}

static std::vector<double> readDoubleVar(int ncid, std::string name, size_t len) {
  int retval, varid;
  if ((retval = nc_inq_varid(ncid, name.c_str(), &varid)) != NC_NOERR) {
    fprintf(stderr, "variable %s not found\n", name.c_str());
    return {};
  }

  std::vector<double> result(len);

  if ((retval = nc_get_var_double(ncid, varid, result.data())) != NC_NOERR) {
    fprintf(stderr, "cannot read from variable %s\n", name.c_str());
    return {};
  }

  if (result.size() != len) {
    fprintf(stderr, "variable %s found but size mismatch\n", name.c_str());
    return {};
  }

  return result;
}


static void parseCommandLine(int argc, char *argv[]) {
  enum Mode { Hgrid, Hsurf, Hhl, Data, None, };
  Mode mode{None};

  for (int i=1; i<argc; ++i) {
    std::string arg = argv[i];
    if (arg[0] != '-') {
      g_appState.inputFile = arg;
    }
    else if (arg == "-img") {
      for (int j=0;j<2;++j) {
        g_appState.imgSize[j] = std::stoi(argv[++i]);
      }
    }
    else if (arg == "-roi") {
      for (int j=0;j<4;++j) {
        g_appState.roi[j] = std::stoi(argv[++i]);
      }
    }
  }
}

static void printUsage() {
  fprintf(stderr, "%s\n",
    "Usage: ./convert_icon -hgrid <hg.nc> -hsurf <hs.nc> -hhl [hh.nc*] -data [df.nc*]");
}

int main(int argc, char *argv[]) {
  if (argc < 2 || std::string(argv[1]) == "help" ) {
    printHelp();
    return 1;
  }

  parseCommandLine(argc, argv);

  if (g_appState.inputFile.empty()) {
    printUsage();
    return 1;
  }

  std::cout << "Resampled grid size: " << g_appState.imgSize[0] << ','
    << g_appState.imgSize[1] << '\n';

  if (g_appState.roi[0] >= 0) {
    g_appState.roi[2] = std::min(g_appState.roi[2],g_appState.imgSize[0]);
    g_appState.roi[3] = std::min(g_appState.roi[3],g_appState.imgSize[1]);
    std::cout << "Using ROI: " << g_appState.roi[0] << ',' << g_appState.roi[1] << " -- "
      << g_appState.roi[2] << ',' << g_appState.roi[3] << '\n';
  }

  int ncid, retval;

  // Horizontal grid file:

  if ((retval = nc_open(g_appState.inputFile.c_str(), NC_NOWRITE, &ncid)) != NC_NOERR) {
    printf("Error opening file: %s\n", nc_strerror(retval));
    return 1;
  }

  // read number of cells:
  size_t height_2Dim = readDimLength(ncid, "height_2");
  size_t timeDim = readDimLength(ncid, "time");
  printf("height_2 dimension: %i\n",(int)height_2Dim);
  printf("time dimension: %i\n",(int)timeDim);

  // to-do: why does nc_inq_dimid reutrn "op not permitted"??
  // (we'll just hardcode these for now..)
  auto time = readDoubleVar(ncid, "time", timeDim);
  auto height_2 = readDoubleVar(ncid, "height_2", height_2Dim);
  auto var = readDoubleVar(ncid, g_appState.varName.c_str(), timeDim*height_2Dim);

  // might just assume these are sorted.. but, better be on the safe side:
  float height_2Min{FLT_MAX}, height_2Max{-FLT_MAX};
  float timeMin{FLT_MAX}, timeMax{-FLT_MAX};
  float varMin{FLT_MAX}, varMax{-FLT_MAX};

  for (int i=0; i<time.size(); ++i) {
    timeMin = fminf(timeMin,time[i]);
    timeMax = fmaxf(timeMax,time[i]);
  }

  for (int i=0; i<height_2.size(); ++i) {
    height_2Min = fminf(height_2Min,height_2[i]);
    height_2Max = fmaxf(height_2Max,height_2[i]);
  }

  for (int i=0; i<var.size(); ++i) {
    varMin = fminf(varMin,var[i]);
    varMax = fmaxf(varMax,var[i]);
  }

  printf("time (min/max): %f:%f\n",timeMin,timeMax);
  printf("height_2 (min/max): %f:%f\n",height_2Min,height_2Max);
  printf("var (min/max): %f:%f\n",varMin,varMax);

  #define NUM_BINS 1

  std::vector<float> pixels(g_appState.imgSize[0]*size_t(g_appState.imgSize[1])*NUM_BINS,0);
  std::vector<float> sumWeights(g_appState.imgSize[0]*size_t(g_appState.imgSize[1])*NUM_BINS,0);

  for (int h=0; h<height_2Dim-1; ++h) {
    for (int t=0; t<timeDim-1; ++t) {
      // compute normalized: coordinates in x/y/bin

      float vv = var[t*height_2Dim+h];

      // lo
      float xf1 = (time[t]-timeMin)/(timeMax-timeMin);
      float yf1 = (height_2[h]-height_2Min)/(height_2Max-height_2Min);
      float bf1 = (vv-varMin)/(varMax-varMin);
      // hi
      float xf2 = (time[t+1]-timeMin)/(timeMax-timeMin);
      float yf2 = (height_2[h+1]-height_2Min)/(height_2Max-height_2Min);
      float bf2 = (vv-varMin)/(varMax-varMin);

      int x1 = xf1*(g_appState.imgSize[0]-1);
      int y1 = yf1*(g_appState.imgSize[1]-1);
      int z1 = bf1*(NUM_BINS-1);

      int x2 = xf2*(g_appState.imgSize[0]-1);
      int y2 = yf2*(g_appState.imgSize[1]-1);
      int z2 = bf2*(NUM_BINS-1);

      if (1) { // tent
        int z_lo=std::min(z1,z2), z_hi=std::max(z1,z2);
        int y_lo=std::min(y1,y2), y_hi=std::max(y1,y2);
        int x_lo=std::min(x1,x2), x_hi=std::max(x1,x2);
        y_lo -= ceilf((y_hi-y_lo+1)*0.5f);
        y_hi += ceilf((y_hi-y_lo+1)*0.5f);
        x_lo -= ceilf((x_hi-x_lo+1)*0.5f);
        x_hi += ceilf((x_hi-x_lo+1)*0.5f);
        for (int pz=z_lo;pz<=z_hi;++pz) {
          for (int py=y_lo;py<=y_hi;++py) {
            for (int px=x_lo;px<=x_hi;++px) {
              if (px<0||py<0||pz<0 || px>=g_appState.imgSize[0]
                  || py>=g_appState.imgSize[1] || pz>=NUM_BINS) {
                continue;
              }
              int pixelID = pz*size_t(g_appState.imgSize[0])*g_appState.imgSize[1]
                                +py*g_appState.imgSize[0]
                                +px;
              float fx = (px-x_lo)/float(x_hi-x_lo);
              if (fx > 0.5f) fx = 1.f-fx;

              float fy = (py-y_lo)/float(y_hi-y_lo);
              if (fy > 0.5f) fy = 1.f-fy;

              float weight = fx*fy;
              pixels[pixelID] += vv * weight;
              sumWeights[pixelID] += weight;
            }
          }
        }
      } else { // box
        for (int pz=std::min(z1,z2);pz<=std::max(z1,z2);++pz) {
          for (int py=std::min(y1,y2);py<=std::max(y1,y2);++py) {
            for (int px=std::min(x1,x2);px<=std::max(x1,x2);++px) {
              int pixelID = pz*size_t(g_appState.imgSize[0])*g_appState.imgSize[1]
                                +py*g_appState.imgSize[0]
                                +px;
              pixels[pixelID] += vv;
              sumWeights[pixelID] += 1.f;
            }
          }
        }
      }
    }
  }

  for (size_t i=0; i<pixels.size(); ++i) {
    if (sumWeights[i] == 0.f) continue;
    pixels[i] /= sumWeights[i];
  }

  if (g_appState.outputBinary) {
    // TODO: apply ROI
    std::stringstream filename;
    filename << g_appState.varName << '_' << g_appState.imgSize[0] << 'x' << g_appState.imgSize[1] << 'x' << NUM_BINS << "_float32.raw";
    std::ofstream out(filename.str(),std::ios::binary);
    out.write((const char *)pixels.data(),pixels.size()*sizeof(float));
  }

  if (g_appState.outputText) {
    std::ofstream out("out.txt");
    out << std::setprecision(5);
    out << std::fixed;
    //out << NUM_BINS << ' ' << g_appState.imgSize[1] << ' ' << g_appState.imgSize[0] << '\n';
    int dimY = g_appState.imgSize[1];
    int dimX = g_appState.imgSize[0];
    int y_lo=0, y_hi=g_appState.imgSize[1];
    int x_lo=0, x_hi=g_appState.imgSize[0];
    if (g_appState.roi[0] >= 0) {
      dimY = g_appState.roi[3]-g_appState.roi[1];
      dimX = g_appState.roi[2]-g_appState.roi[0];
      y_lo = g_appState.roi[1], y_hi = g_appState.roi[3];
      x_lo = g_appState.roi[0], x_hi = g_appState.roi[2];
    }
    out << dimY << ' ' << dimX << '\n'; // Hamish's tool wants 2D dims!
    for (int z=0; z<NUM_BINS; ++z) {
      for (int y=y_lo; y<y_hi; ++y) {
        for (int x=x_lo; x<x_hi; ++x) {
          out << pixels[z*size_t(g_appState.imgSize[0])*g_appState.imgSize[1]+y*g_appState.imgSize[0]+x] << ' ';
        }
        out << '\n';
      }
      out << '\n';
    }
  }

  if (g_appState.outputLegacyVTK) {
    std::ofstream out("out.vtk");
    out << std::setprecision(5);
    out << std::fixed;
    //out << NUM_BINS << ' ' << g_appState.imgSize[1] << ' ' << g_appState.imgSize[0] << '\n';
    int dimY = g_appState.imgSize[1];
    int dimX = g_appState.imgSize[0];
    int y_lo=0, y_hi=g_appState.imgSize[1];
    int x_lo=0, x_hi=g_appState.imgSize[0];
    if (g_appState.roi[0] >= 0) {
      dimY = g_appState.roi[3]-g_appState.roi[1];
      dimX = g_appState.roi[2]-g_appState.roi[0];
      y_lo = g_appState.roi[1], y_hi = g_appState.roi[3];
      x_lo = g_appState.roi[0], x_hi = g_appState.roi[2];
    }
    out << R"(# vtk DataFile Version 3.0
2D Structured Grid Data
ASCII
DATASET STRUCTURED_POINTS)";
    out <<
      "\nDIMENSIONS " << dimX << ' ' << dimY << ' ' << 1 << '\n';
    out << R"(
ORIGIN 0 0 0
SPACING 1 1 1)";

    out <<
      "\nPOINT_DATA " << dimX*dimY << '\n';
    out << R"(SCALARS MyScalars float 1
LOOKUP_TABLE default)";
    out << '\n';
    for (int z=0; z<NUM_BINS; ++z) {
      for (int y=y_lo; y<y_hi; ++y) {
        for (int x=x_lo; x<x_hi; ++x) {
          out << pixels[z*size_t(g_appState.imgSize[0])*g_appState.imgSize[1]+y*g_appState.imgSize[0]+x] << ' ';
        }
      }
    }
  }
}







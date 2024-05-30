#include "lib/nanoparquet.h"
#include "lib/bitpacker.h"

#include <Rdefines.h>

using namespace nanoparquet;
using namespace std;

class RParquetOutFile : public ParquetOutFile {
public:
  RParquetOutFile(
    std::string filename,
    parquet::format::CompressionCodec::type codec
  );
  void write_int32(std::ostream &file, uint32_t idx, uint64_t from,
                   uint64_t until);
  void write_int64(std::ostream &file, uint32_t idx, uint64_t from,
                   uint64_t until);
  void write_double(std::ostream &file, uint32_t idx, uint64_t from,
                    uint64_t until);
  void write_byte_array(std::ostream &file, uint32_t id, uint64_t from,
                        uint64_t until);
  uint32_t get_size_byte_array(uint32_t idx, uint32_t num_present,
                               uint64_t from, uint64_t until);
  void write_boolean(std::ostream &file, uint32_t idx, uint64_t from,
                     uint64_t until);

  uint32_t write_present(std::ostream &file, uint32_t idx, uint64_t from,
                         uint64_t until);
  void write_present_int32(std::ostream &file, uint32_t idx,
                           uint32_t num_present, uint64_t from, uint64_t until);
  void write_present_int64(std::ostream &file, uint32_t idx,
                           uint32_t num_present, uint64_t from, uint64_t until);
  void write_present_double(std::ostream &file, uint32_t idx,
                            uint32_t num_present, uint64_t from,
                            uint64_t until);
  void write_present_byte_array(std::ostream &file, uint32_t idx,
                                uint32_t num_present, uint64_t from,
                                uint64_t until);
  void write_present_boolean(std::ostream &file, uint32_t idx,
                             uint32_t num_present, uint64_t from,
                             uint64_t until);

  // for factors
  uint32_t get_num_values_byte_array_dictionary(uint32_t idx);
  uint32_t get_size_byte_array_dictionary(uint32_t idx);
  void write_byte_array_dictionary(std::ostream &file, uint32_t idx);
  void write_dictionary_indices(std::ostream &file, uint32_t idx,
                                uint64_t from, uint64_t until);

  void write(SEXP dfsxp, SEXP dim, SEXP metadata, SEXP rrequired);

private:
  SEXP df = R_NilValue;
  SEXP required = R_NilValue;
  ByteBuffer present;
};

RParquetOutFile::RParquetOutFile(
  std::string filename,
  parquet::format::CompressionCodec::type codec
) : ParquetOutFile(filename, codec) {
  // nothing to do here
}

void RParquetOutFile::write_int32(std::ostream &file, uint32_t idx,
                                  uint64_t from, uint64_t until) {
  SEXP col = VECTOR_ELT(df, idx);
  if (until > Rf_xlength(col)) {
    Rf_error("Internal nanoparquet error, row index too large");
  }
  uint64_t len = until - from;
  file.write((const char *) (INTEGER(col) + from), sizeof(int) * len);
}

void RParquetOutFile::write_int64(std::ostream &file, uint32_t idx,
                                  uint64_t from, uint64_t until) {
  // This is double in R, so we need to convert
  SEXP col = VECTOR_ELT(df, idx);
  if (until > Rf_xlength(col)) {
    Rf_error("Internal nanoparquet error, row index too large");
  }
  if (Rf_inherits(col, "POSIXct")) {
    // need to convert seconds to microseconds
    for (uint64_t i = from; i < until; i++) {
      int64_t el = REAL(col)[i] * 1000 * 1000;
      file.write((const char*) &el, sizeof(int64_t));
    }

  } else if (Rf_inherits(col, "difftime")) {
    // need to convert seconds to nanoseconds
    for (uint64_t i = from; i < until; i++) {
      int64_t el = REAL(col)[i] * 1000 * 1000 * 1000;
      file.write((const char*) &el, sizeof(int64_t));
    }

  } else {
    for (uint64_t i = from; i < until; i++) {
      int64_t el = REAL(col)[i];
      file.write((const char*) &el, sizeof(int64_t));
    }
  }
}

void RParquetOutFile::write_double(std::ostream &file, uint32_t idx,
                                   uint64_t from, uint64_t until) {
  SEXP col = VECTOR_ELT(df, idx);
  if (until > Rf_xlength(col)) {
    Rf_error("Internal nanoparquet error, row index too large");
  }
  uint64_t len = until - from;
  file.write((const char *) (REAL(col) + from), sizeof(double) * len);
}

void RParquetOutFile::write_byte_array(std::ostream &file, uint32_t idx,
                                       uint64_t from, uint64_t until) {
  SEXP col = VECTOR_ELT(df, idx);
  if (until > Rf_xlength(col)) {
    Rf_error("Internal nanoparquet error, row index too large");
  }
  for (uint64_t i = from; i < until; i++) {
    const char *c = CHAR(STRING_ELT(col, i));
    uint32_t len1 = strlen(c);
    file.write((const char *)&len1, 4);
    file.write(c, len1);
  }
}

uint32_t RParquetOutFile::get_size_byte_array(
  uint32_t idx,
  uint32_t num_present,
  uint64_t from,
  uint64_t until) {

  SEXP col = VECTOR_ELT(df, idx);
  if (until > Rf_xlength(col)) {
    Rf_error("Internal nanoparquet error, row index too large");
  }
  uint32_t size = 0;
  for (uint64_t i = from; i < until; i++) {
    SEXP csxp = STRING_ELT(col, i);
    if (csxp != NA_STRING) {
      const char *c = CHAR(csxp);
      size += strlen(c) + 4;
    }
  }
  return size;
}

void write_boolean_impl(std::ostream &file, SEXP col,
                        uint64_t from, uint64_t until) {
  if (until > Rf_xlength(col)) {
    Rf_error("Internal nanoparquet error, row index too large");
  }
  uint64_t len = until - from;
  int *p = LOGICAL(col) + from;
  int *end = p + len;
  while (p + 8 < end) {
    char b = 0;
    b += (*p++);
    b += (*p++) << 1;
    b += (*p++) << 2;
    b += (*p++) << 3;
    b += (*p++) << 4;
    b += (*p++) << 5;
    b += (*p++) << 6;
    b += (*p++) << 7;
    file.write(&b, 1);
  }
  if (p < end) {
    char b = 0;
    while (end > p) {
      b = (b << 1) + (*--end);
    }
    file.write(&b, 1);
  }
}

void RParquetOutFile::write_boolean(std::ostream &file, uint32_t idx,
                                    uint64_t from, uint64_t until) {
  SEXP col = VECTOR_ELT(df, idx);
  write_boolean_impl(file, col, from, until);
}

uint32_t RParquetOutFile:: write_present(std::ostream &file, uint32_t idx,
                                         uint64_t from, uint64_t until) {
  SEXP col = VECTOR_ELT(df, idx);
  if (until > Rf_xlength(col)) {
    Rf_error("Internal nanoparquet error, row index too large");
  }
  uint64_t len = until - from;
  uint64_t num_pres = 0;
  present.reset(len * sizeof(int));
  int *presptr = (int*) present.ptr;
  switch(TYPEOF(col)) {
  case INTSXP: {
    int *intptr = INTEGER(col) + from;
    int *end = intptr + len;
    for (; intptr < end; intptr++, presptr++) {
      *presptr = (*intptr != NA_INTEGER);
      num_pres += *presptr;
    }
    break;
  }
  case REALSXP: {
    double *dblptr = REAL(col) + from;
    double *end = dblptr + len;
    for (; dblptr < end; dblptr++, presptr++) {
      *presptr = ! R_IsNA(*dblptr);
      num_pres += *presptr;
    }
    break;
  }
  case STRSXP: {
    for (auto i = from; i < until; i++, presptr++) {
      *presptr = (STRING_ELT(col, i) != R_NaString);
      num_pres += *presptr;
    }
    break;
  }
  case LGLSXP: {
    int *lglptr = LOGICAL(col) + from;
    int *end = lglptr + len;
    for (; lglptr < end; lglptr++, presptr++) {
      *presptr = (*lglptr != NA_LOGICAL);
      num_pres += *presptr;
    }
    break;
  }
  default:
    throw runtime_error("Uninmplemented R type");
  }

  file.write(present.ptr, len * sizeof(int));

  return num_pres;
}

void RParquetOutFile::write_present_int32(
  std::ostream &file,
  uint32_t idx,
  uint32_t num_present,
  uint64_t from,
  uint64_t until) {

  SEXP col = VECTOR_ELT(df, idx);
  if (until > Rf_xlength(col)) {
    Rf_error("Internal nanoparquet error, row index too large");
  }
  for (uint64_t i = from; i < until; i++) {
    int el = INTEGER(col)[i];
    if (el != NA_INTEGER) {
      file.write((const char*) &el, sizeof(int));
    }
  }
}

void RParquetOutFile::write_present_int64(
  std::ostream &file,
  uint32_t idx,
  uint32_t num_present,
  uint64_t from,
  uint64_t until) {

  SEXP col = VECTOR_ELT(df, idx);
  if (until > Rf_xlength(col)) {
    Rf_error("Internal nanoparquet error, row index too large");
  }
  if (Rf_inherits(col, "POSIXct")) {
    for (uint64_t i = from; i < until; i++) {
      double el = REAL(col)[i];
      if (R_IsNA(el)) continue;
      int64_t el2 = el * 1000 * 1000;
      file.write((const char*) &el2, sizeof(int64_t));
    }
  } else if (Rf_inherits(col, "difftime")) {
    // need to convert seconds to nanoseconds
    for (uint64_t i = from; i < until; i++) {
      double el = REAL(col)[i];
      if (R_IsNA(el)) continue;
      int64_t el2 = el * 1000 * 1000 * 1000;
      file.write((const char*) &el2, sizeof(int64_t));
    }

  } else {
    for (uint64_t i = from; i < until; i++) {
      double el = REAL(col)[i];
      if (R_IsNA(el)) continue;
      int64_t el2 = el;
      file.write((const char*) &el2, sizeof(int64_t));
    }
  }
}

void RParquetOutFile::write_present_double(
  std::ostream &file,
  uint32_t idx,
  uint32_t num_present,
  uint64_t from,
  uint64_t until) {

  SEXP col = VECTOR_ELT(df, idx);
  if (until > Rf_xlength(col)) {
    Rf_error("Internal nanoparquet error, row index too large");
  }
  for (uint64_t i = from; i < until; i++) {
    double el = REAL(col)[i];
    if (!R_IsNA(el)) {
      file.write((const char*) &el, sizeof(double));
    }
  }
}

void RParquetOutFile::write_present_byte_array(
  std::ostream &file,
  uint32_t idx,
  uint32_t num_present,
  uint64_t from,
  uint64_t until) {

  SEXP col = VECTOR_ELT(df, idx);
  if (until > Rf_xlength(col)) {
    Rf_error("Internal nanoparquet error, row index too large");
  }
  for (uint64_t i = from; i < until; i++) {
    SEXP csxp = STRING_ELT(col, i);
    if (csxp != R_NaString) {
      const char *c = CHAR(csxp);
      uint32_t len1 = strlen(c);
      file.write((const char *)&len1, 4);
      file.write(c, len1);
    }
  }
}

void RParquetOutFile::write_present_boolean(
  std::ostream &file,
  uint32_t idx,
  uint32_t num_present,
  uint64_t from,
  uint64_t until) {

  SEXP col = VECTOR_ELT(df, idx);
  SEXP col2 = PROTECT(Rf_allocVector(LGLSXP, num_present));
  uint64_t i, o;
  if (until > Rf_xlength(col)) {
    Rf_error("Internal nanoparquet error, row index too large");
  }
  for (i = from, o = 0; i < until; i++) {
    if (LOGICAL(col)[i] != NA_LOGICAL) {
      LOGICAL(col2)[o++] = LOGICAL(col)[i];
    }
  }
  write_boolean_impl(file, col2, 0, num_present);

  UNPROTECT(1);
}

uint32_t RParquetOutFile::get_num_values_byte_array_dictionary(
    uint32_t idx) {
  SEXP col = VECTOR_ELT(df, idx);
  return Rf_nlevels(col);
}

uint32_t RParquetOutFile::get_size_byte_array_dictionary(uint32_t idx) {
  SEXP col = VECTOR_ELT(df, idx);
  SEXP levels = PROTECT(Rf_getAttrib(col, R_LevelsSymbol));
  R_xlen_t len = Rf_xlength(levels);
  uint32_t size = len * 4; // 4 bytes of length for each CHARSXP
  for (R_xlen_t i = 0; i < len; i++) {
    const char *c = CHAR(STRING_ELT(levels, i));
    size += strlen(c);
  }
  UNPROTECT(1);
  return size;
}

void RParquetOutFile::write_byte_array_dictionary(
    std::ostream &file,
    uint32_t idx) {
  SEXP col = VECTOR_ELT(df, idx);
  SEXP levels = PROTECT(Rf_getAttrib(col, R_LevelsSymbol));
  R_xlen_t len = XLENGTH(levels);
  for (R_xlen_t i = 0; i < len; i++) {
    const char *c = CHAR(STRING_ELT(levels, i));
    uint32_t len1 = strlen(c);
    file.write((const char *)&len1, 4);
    file.write(c, len1);
  }
  UNPROTECT(1);
}

void RParquetOutFile::write_dictionary_indices(
  std::ostream &file,
  uint32_t idx,
  uint64_t from,
  uint64_t until) {

  SEXP col = VECTOR_ELT(df, idx);
  if (until > Rf_xlength(col)) {
    Rf_error("Internal nanoparquet error, row index too large");
  }
  for (uint64_t i = from; i < until; i++) {
    int el = INTEGER(col)[i];
    if (el != NA_INTEGER) {
      el--;
      file.write((const char *) &el, sizeof(int));
    }
  }
}

void RParquetOutFile::write(
    SEXP dfsxp,
    SEXP dim,
    SEXP metadata,
    SEXP rrequired) {
  df = dfsxp;
  required = rrequired;
  SEXP nms = PROTECT(Rf_getAttrib(dfsxp, R_NamesSymbol));
  R_xlen_t nr = INTEGER(dim)[0];
  set_num_rows(nr);
  R_xlen_t nc = INTEGER(dim)[1];
  for (R_xlen_t idx = 0; idx < nc; idx++) {
    SEXP col = VECTOR_ELT(dfsxp, idx);
    int rtype = TYPEOF(col);
    bool req = LOGICAL(required)[idx];
    switch (rtype) {
    case INTSXP: {
      if (Rf_isFactor(col)) {
        parquet::format::StringType st;
        parquet::format::LogicalType logical_type;
        logical_type.__set_STRING(st);
        schema_add_column(CHAR(STRING_ELT(nms, idx)), logical_type, req, true);
      } else if (Rf_inherits(col, "Date")) {
        parquet::format::DateType dt;
        parquet::format::LogicalType logical_type;
        logical_type.__set_DATE(dt);
        schema_add_column(CHAR(STRING_ELT(nms, idx)), logical_type, req);
      } else if (Rf_inherits(col, "hms")) {
        parquet::format::TimeUnit tu;
        tu.__set_MILLIS(parquet::format::MilliSeconds());
        parquet::format::TimeType tt;
        tt.__set_isAdjustedToUTC(true);
        tt.__set_unit(tu);
        parquet::format::LogicalType logical_type;
        logical_type.__set_TIME(tt);
        schema_add_column(CHAR(STRING_ELT(nms, idx)), logical_type, req);
      } else {
        parquet::format::IntType it;
        it.__set_isSigned(true);
        it.__set_bitWidth(32);
        parquet::format::LogicalType logical_type;
        logical_type.__set_INTEGER(it);
        schema_add_column(CHAR(STRING_ELT(nms, idx)), logical_type, req);
      }
      break;
    }
    case REALSXP: {
      if (Rf_inherits(col, "POSIXct")) {
        parquet::format::TimeUnit tu;
        tu.__set_MICROS(parquet::format::MicroSeconds());
        parquet::format::TimestampType ttt;
        ttt.__set_isAdjustedToUTC(true);
        ttt.__set_unit(tu);
        parquet::format::LogicalType logical_type;
        logical_type.__set_TIMESTAMP(ttt);
        schema_add_column(CHAR(STRING_ELT(nms, idx)), logical_type, req);
      } else if (Rf_inherits(col, "difftime")) {
        parquet::format::Type::type type = parquet::format::Type::INT64;
        schema_add_column(CHAR(STRING_ELT(nms, idx)), type, req);

      } else {
        parquet::format::Type::type type = parquet::format::Type::DOUBLE;
        schema_add_column(CHAR(STRING_ELT(nms, idx)), type, req);
      }
      break;
    }
    case STRSXP: {
      parquet::format::StringType st;
      parquet::format::LogicalType logical_type;
      logical_type.__set_STRING(st);
      schema_add_column(CHAR(STRING_ELT(nms, idx)), logical_type, req);
      break;
    }
    case LGLSXP: {
      parquet::format::Type::type type = parquet::format::Type::BOOLEAN;
      schema_add_column(CHAR(STRING_ELT(nms, idx)), type, req);
      break;
    }
    default:
      throw runtime_error("Uninmplemented R type");  // # nocov
    }
  }

  if (!Rf_isNull(metadata)) {
    SEXP keys = VECTOR_ELT(metadata, 0);
    SEXP vals = VECTOR_ELT(metadata, 1);
    R_xlen_t len = Rf_xlength(keys);
    for (auto i = 0; i < len; i++) {
      add_key_value_metadata(
        CHAR(STRING_ELT(keys, i)),
        CHAR(STRING_ELT(vals, i))
      );
    }
  }

  ParquetOutFile::write();

  UNPROTECT(1);
}

extern "C" {

SEXP nanoparquet_write(
  SEXP dfsxp,
  SEXP filesxp,
  SEXP dim,
  SEXP compression,
  SEXP metadata,
  SEXP required) {

  if (TYPEOF(filesxp) != STRSXP || LENGTH(filesxp) != 1) {
    Rf_error("nanoparquet_write: filename must be a string"); // # nocov
  }

  int c_compression = INTEGER(compression)[0];
  parquet::format::CompressionCodec::type codec;
  switch(c_compression) {
    case 0:
      codec = parquet::format::CompressionCodec::UNCOMPRESSED;
      break;
    case 1:
      codec = parquet::format::CompressionCodec::SNAPPY;
      break;
    default:
      Rf_error("Invalid compression type code: %d", c_compression); // # nocov
      break;
  }

  char error_buffer[8192];
  error_buffer[0] = '\0';

  try {
    char *fname = (char *)CHAR(STRING_ELT(filesxp, 0));
    RParquetOutFile of(fname, codec);
    of.write(dfsxp, dim, metadata, required);
    return R_NilValue;

  } catch (std::exception &ex) {
    strncpy(error_buffer, ex.what(), sizeof(error_buffer) - 1); // # nocov
  }

  if (error_buffer[0] != '\0') {         // # nocov
    Rf_error("%s", error_buffer);        // # nocov
  }                                      // # nocov

  // never reached
  return R_NilValue; // # nocov
}

} // extern "C"
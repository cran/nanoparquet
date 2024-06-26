% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/porcelain.R
\name{rle_decode_int}
\alias{rle_decode_int}
\title{RLE decode integers}
\usage{
rle_decode_int(
  x,
  bit_width = attr(x, "bit_width"),
  length = attr(x, "length") \%||\% NA
)
}
\arguments{
\item{x}{Raw vector of the encoded integers.}

\item{bit_width}{Bit width used for the encoding.}

\item{length}{Length of the output. If \code{NA} then we assume that \code{x}
starts with length of the output, encoded as a 4 byte integer.}
}
\value{
The decoded integer vector.
}
\description{
RLE decode integers
}
\seealso{
\code{\link[=rle_encode_int]{rle_encode_int()}}

Other encodings: 
\code{\link{rle_encode_int}()}
}
\concept{encodings}
\keyword{internal}

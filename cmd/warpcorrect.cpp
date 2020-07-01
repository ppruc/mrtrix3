/* Copyright (c) 2008-2020 the MRtrix3 contributors.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Covered Software is provided under this License on an "as is"
 * basis, without warranty of any kind, either expressed, implied, or
 * statutory, including, without limitation, warranties that the
 * Covered Software is free of defects, merchantable, fit for a
 * particular purpose or non-infringing.
 * See the Mozilla Public License v. 2.0 for more details.
 *
 * For more details, see http://www.mrtrix.org/.
 */
#include "command.h"
#include "image.h"
#include "algo/threaded_loop.h"
#include "registration/warp/helpers.h"


using namespace MR;
using namespace App;


const float PRECISION = Eigen::NumTraits<float>::dummy_precision();

void usage ()
{
  AUTHOR = "David Raffelt (david.raffelt@florey.edu.au) & Max Pietsch (mail@maxpietsch.com)";

  SYNOPSIS = "Replaces voxels in a deformation field that point to a specific out of bounds location with nan,nan,nan";

  DESCRIPTION
  + "This can be used in conjunction with the warpinit command to compute a MRtrix "
    "compatible deformation field from non-linear transformations generated by any other registration package.";

  ARGUMENTS
  + Argument ("in", "the input warp image.").type_image_in ()
  + Argument ("out", "the output warp image.").type_image_out ();

  OPTIONS
  + Option ("marker", "single value or a comma separated list of values that define out of bounds voxels in the input warp image."
    " Default: (0,0,0).")
    + Argument ("coordinates").type_sequence_float()
  + Option ("tolerance", "numerical precision used for L2 matrix norm comparison. Default: " + str(PRECISION) + ".")
    + Argument ("value").type_float(PRECISION);
}


using value_type = float;

class BoundsCheck { MEMALIGN(BoundsCheck)
  public:
    BoundsCheck (value_type tolerance, const Eigen::Matrix<value_type, 3, 1>& marker, size_t& total_count):
     precision (tolerance),
     vec (marker),
     counter (total_count),
     count (0) { }
    template <class ImageTypeIn, class ImageTypeOut>
      void operator() (ImageTypeIn& in, ImageTypeOut& out)
      {
        val = Eigen::Matrix<value_type, 3, 1>(in.row(3));
        if ((vec - val).isMuchSmallerThan(precision) || (vec.hasNaN() && val.hasNaN())) {
          count++;
          for (auto l = Loop (3) (out); l; ++l)
            out.value() = NaN;
        } else {
          for (auto l = Loop (3) (in, out); l; ++l)
            out.value() = in.value();
        }
      }
    virtual ~BoundsCheck () {
      counter += count;
    }
  protected:
    const value_type precision;
    const Eigen::Matrix<value_type, 3, 1> vec;
    size_t& counter;
    size_t count;
    Eigen::Matrix<value_type, 3, 1> val;
};


void run ()
{
  auto input = Image<value_type>::open (argument[0]).with_direct_io (3);
  Registration::Warp::check_warp (input);

  auto output = Image<value_type>::create (argument[1], input);

  Eigen::Matrix<value_type,3,1> oob_vector = Eigen::Matrix<value_type,3,1>::Zero();
  auto opt = get_options ("marker");
  if (opt.size() == 1) {
    const auto loc = parse_floats (opt[0][0]);
    if (loc.size() == 1) {
      oob_vector.fill(loc[0]);
    } else if (loc.size() == 3) {
      for (auto i=0; i<3; i++)
        oob_vector[i] = loc[i];
    } else throw Exception("location option requires either single value or list of 3 values");
  }

  opt = get_options ("tolerance");
  value_type precision = PRECISION;
  if (opt.size())
    precision = opt[0][0];

  size_t count (0);
  auto func = BoundsCheck (precision, oob_vector, count);

  ThreadedLoop ("correcting warp", input, 0, 3)
    .run (func, input, output);

  if (count == 0)
    WARN("no out of bounds voxels found with value (" +
      str(oob_vector[0]) + "," + str(oob_vector[1]) + "," + str(oob_vector[2]) + ")");
  INFO("converted " + str(count) + " out of bounds values");
}

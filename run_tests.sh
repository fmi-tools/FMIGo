# This runs all test scripts we have scattered all over the place
set -e
source boilerplate.sh

if [ $USE_GPL -eq 1 ]
then
  (cd tests/umit-fmus/me              && ( ./test_me.sh ||  ( echo "failed modelExchange" && exit 1 ) ) )
  # wrap me/springs2.fmu with filter enabled
  python $WRAPPER -f ${FMUS_DIR}/me/springs2/springs2.fmu ${BUILD_DIR}/springs2_wrapped_filter.fmu
  (cd tests/splitting/python && ( python truckstring.py --test  || ( echo "failed truckstring test" && exit 1 ) ) )
fi
(cd tests/work-reports       && ( ./run_tests.sh  || ( echo "failed tests in work-reports" && exit 1 ) ) )
(cd tests/umit-fmus/tests             && ( ./run_tests.sh ||  ( echo "failed umit-fmus tests" && exit 1 ) ) )
(cd ${BUILD_DIR}                && ( ctest --output-on-failure || ( echo "ctest failed" && exit 1 ) ) )
(cd tests/umit-fmus/meWrapper         &&( ./test_wrapper.sh ||  ( echo "failed wrapper" && exit 1 ) ) )

# Check -f none
touch empty_file
mpiexec -np 2 fmigo-mpi -f none ${FMUS_DIR}/gsl/clutch/clutch.fmu | diff empty_file -
rm empty_file
echo Option \"-f none\" works correctly

# Check that bad parameters give expected failure
# First clutch is run with setting VRs which don't exist, but with proper values
# Then it is run with values which are just plain wrong
# The log is grep'd for the expected errors
for t in "r,0,1234,111" "i,0,1234,111" "b,0,1234,true" "s,0,1234,111"
do
  #                                                                          Success = failure
  mpiexec -np 2 fmigo-mpi -p $t ${FMUS_DIR}/gsl/clutch/clutch.fmu &> temp && exit 1         || echo -n
  grep "Couldn't find variable"   temp > /dev/null
  echo Setting incorrect $t fails as expected
done
for t in "0,x0_e,notreal" "0,gear,notint" "0,octave_output,notbool"
do
  #                                                                          Success = failure
  mpiexec -np 2 fmigo-mpi -p $t ${FMUS_DIR}/gsl/clutch/clutch.fmu &> temp && exit 1         || echo -n
  grep "does not look like a"   temp > /dev/null
  echo Setting incorrect $t fails as expected
done
rm temp

# Test wrapper, both Debug and Release
python $WRAPPER -t Debug   ${FMUS_DIR}/me/bouncingBall/bouncingBall.fmu ${BUILD_DIR}/bouncingBall_wrapped_Debug.fmu
python $WRAPPER -t Release ${FMUS_DIR}/me/bouncingBall/bouncingBall.fmu ${BUILD_DIR}/bouncingBall_wrapped_Release.fmu
python $WRAPPER -f -t Debug   ${FMUS_DIR}/me/bouncingBall/bouncingBall.fmu ${BUILD_DIR}/bouncingBall_wrapped_filter_Debug.fmu
python $WRAPPER -f -t Release ${FMUS_DIR}/me/bouncingBall/bouncingBall.fmu ${BUILD_DIR}/bouncingBall_wrapped_filter_Release.fmu

# Test -d option for adding resources/directional.txt
python $WRAPPER -d "0 1 2" -d "3 4 5" ${FMUS_DIR}/me/bouncingBall/bouncingBall.fmu ${BUILD_DIR}/bouncingBall_wrapped_directional.fmu

# Can't do the following on msys2 bash + python2/3
# For some reason python wants unmangled filenames
if [[ ! "$(uname)" = *"MINGW"* ]]
then
  # Check that the file actually has directional.txt and the source FMU in it
  python <<EOF
import zipfile
z = zipfile.ZipFile('${BUILD_DIR}/bouncingBall_wrapped_directional.fmu', 'r')
for f in ['resources/directional.txt', 'resources/bouncingBall.fmu']:
  if not f in z.namelist():
    print('No %s in wrapped FMU' % f)
    exit(1)
EOF
fi

# Give wrapped FMUs a test run
for f in bouncingBall_wrapped_Debug.fmu bouncingBall_wrapped_Release.fmu bouncingBall_wrapped_directional.fmu \
 bouncingBall_wrapped_filter_Debug.fmu bouncingBall_wrapped_filter_Release.fmu
do
  echo Making sure $f runs
  mpiexec -np 2 fmigo-mpi ${BUILD_DIR}/$f > /dev/null
done

# Test alternative MPI command line
mpiexec -np 1 fmigo-mpi -f none \
  : -np 1 fmigo-mpi ${FMUS_DIR}/gsl/clutch/clutch.fmu
  : -np 1 fmigo-mpi ${FMUS_DIR}/gsl/clutch/clutch.fmu

# Test ZMQ control + starting paused
# "python" instead of "python2" or "python3" to pick up the right variant
# on trusty vs xenial for control_pb2.py to work.
(cd tests && python test_control.py)

echo All tests OK

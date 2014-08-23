#include <stdint.h>
#include <ocl.h>

int
main (int argc, const char **argv)
{
    OclPlatform *ocl;
    cl_int errcode;
    cl_program program;
    const char *inname;
    const char *outname;
    size_t asize;
    size_t *sizes;
    char **binaries;

    if (argc < 2) {
        printf ("Usage: dump KERNELS.cl [OUTPUT]\n");
        exit (0);
    }

    inname = argv[1];
    outname = argc == 3 ? argv[2] : "program";

    ocl = ocl_new (0, CL_DEVICE_TYPE_GPU);
    asize = sizeof (size_t) * ocl_get_num_devices (ocl);
    sizes = malloc (asize);
    binaries = malloc (sizeof (char *) * ocl_get_num_devices (ocl));

    program = ocl_create_program_from_file (ocl, inname, NULL, &errcode);
    OCL_CHECK_ERROR (errcode);

    OCL_CHECK_ERROR (clGetProgramInfo (program, CL_PROGRAM_BINARY_SIZES, asize, sizes, NULL));
    
    for (int i = 0; i < ocl_get_num_devices (ocl); i++)
        binaries[i] = malloc (sizes[i]);

    OCL_CHECK_ERROR (clGetProgramInfo (program, CL_PROGRAM_BINARIES, 0, binaries, NULL));

    for (int i = 0; i < ocl_get_num_devices (ocl); i++) {
        char fname[256];
        FILE *fp;

        snprintf (fname, 256, "%s.%i", outname, i);
        fp = fopen (fname, "wb");
        fwrite (binaries[0], sizes[i], 1, fp);
        fclose (fp);
    }

    OCL_CHECK_ERROR (clReleaseProgram (program));

    free (sizes);
    ocl_free (ocl);
}

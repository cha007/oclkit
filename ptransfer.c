#include <glib.h>
#include <stdio.h>
#include "ocl.h"

typedef struct {
    OclPlatform *ocl;
    cl_program program;
    cl_kernel kernel;
    cl_command_queue write_queue;
    cl_command_queue compute_queue;
    cl_command_queue read_queue;
    size_t n_elements;
    size_t size;
    float *input;
    float *output;
    cl_mem in_mem;
    cl_mem out_mem;
} Data;

typedef void (*SetupQueueFunc) (Data *data);

static const int N_ITERATIONS = 50;

static void
execute_kernel (Data *data,
                int n_iterations)
{
    cl_int error;
    cl_event read_event;

    OCL_CHECK_ERROR (error);

    read_event = NULL;

    for (int i = 0; i < n_iterations; i++) {
        cl_event write_event;
        cl_event compute_event;
        cl_event *event;
        cl_int event_number;

        event_number = read_event == NULL ? 0 : 1;
        event = read_event == NULL ? NULL : &read_event;

        OCL_CHECK_ERROR (clEnqueueWriteBuffer (data->write_queue, data->out_mem,
                                               CL_FALSE,
                                               0, data->size, data->output,
                                               event_number, event, &write_event));

        if (read_event != NULL)
            OCL_CHECK_ERROR (clReleaseEvent (read_event));

        OCL_CHECK_ERROR (clSetKernelArg (data->kernel, 0, sizeof (cl_mem), &data->in_mem));
        OCL_CHECK_ERROR (clSetKernelArg (data->kernel, 1, sizeof (cl_mem), &data->out_mem));

        OCL_CHECK_ERROR (clEnqueueNDRangeKernel (data->compute_queue, data->kernel,
                                                 1, NULL, &data->n_elements, NULL,
                                                 1, &write_event, &compute_event));

        OCL_CHECK_ERROR (clEnqueueReadBuffer (data->read_queue, data->out_mem,
                                              CL_FALSE,
                                              0, data->size, data->output,
                                              1, &compute_event, &read_event));

        OCL_CHECK_ERROR (clReleaseEvent (write_event));
        OCL_CHECK_ERROR (clReleaseEvent (compute_event));
    }

    OCL_CHECK_ERROR (clWaitForEvents (1, &read_event));
    OCL_CHECK_ERROR (clReleaseEvent (read_event));
}

static Data *
setup_data (OclPlatform *ocl,
            size_t n_elements)
{
    Data *data;
    cl_int errcode;

    data = malloc (sizeof (Data));

    data->ocl = ocl;

    data->compute_queue = NULL;
    data->write_queue = NULL;
    data->read_queue = NULL;

    data->program = ocl_create_program_from_file (ocl, "test.cl", NULL, &errcode);
    OCL_CHECK_ERROR (errcode);

    data->kernel = clCreateKernel (data->program, "run_sin", &errcode);
    OCL_CHECK_ERROR (errcode);

    data->n_elements = n_elements;
    data->size = n_elements * sizeof (float);
    data->input = malloc (data->size);
    data->output = malloc (data->size);

    data->in_mem = clCreateBuffer (ocl_get_context (ocl), CL_MEM_READ_ONLY,
                                   data->size, NULL, &errcode);
    OCL_CHECK_ERROR (errcode);

    data->out_mem = clCreateBuffer (ocl_get_context (ocl), CL_MEM_WRITE_ONLY,
                                    data->size, NULL, &errcode);
    OCL_CHECK_ERROR (errcode);

    return data;
}

static void
free_data (Data *data)
{
    free (data->input);
    free (data->output);

    OCL_CHECK_ERROR (clReleaseMemObject (data->in_mem));
    OCL_CHECK_ERROR (clReleaseMemObject (data->out_mem));
    OCL_CHECK_ERROR (clReleaseKernel (data->kernel));
    OCL_CHECK_ERROR (clReleaseProgram (data->program));

    free (data);
}

static void
teardown_queues (Data *data)
{
    if (data->compute_queue != NULL &&
        data->compute_queue != data->write_queue &&
        data->compute_queue != data->read_queue) {
        OCL_CHECK_ERROR (clReleaseCommandQueue (data->compute_queue));
    }

    if (data->write_queue != NULL &&
        data->write_queue != data->compute_queue &&
        data->write_queue != data->read_queue) {
        OCL_CHECK_ERROR (clReleaseCommandQueue (data->write_queue));
    }

    if (data->read_queue != NULL &&
        data->read_queue != data->compute_queue &&
        data->read_queue != data->write_queue) {
        OCL_CHECK_ERROR (clReleaseCommandQueue (data->read_queue));
    }

    data->compute_queue = NULL;
    data->read_queue = NULL;
    data->write_queue = NULL;
}

static void
setup_single_blocking_queue (Data *data)
{
    cl_int errcode;

    data->compute_queue = clCreateCommandQueue (ocl_get_context (data->ocl),
                                                ocl_get_devices (data->ocl)[0],
                                                0, &errcode);
    OCL_CHECK_ERROR (errcode);
    data->write_queue = data->compute_queue;
    data->read_queue = data->compute_queue;
}

static void
setup_ooo_queue (Data *data)
{
    cl_int errcode;

    data->compute_queue = clCreateCommandQueue (ocl_get_context (data->ocl),
                                                ocl_get_devices (data->ocl)[0],
                                                CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 
                                                &errcode);
    OCL_CHECK_ERROR (errcode);
    data->write_queue = data->compute_queue;
    data->read_queue = data->compute_queue;
}

static void
setup_two_queues (Data *data)
{
    cl_int errcode;

    data->compute_queue = clCreateCommandQueue (ocl_get_context (data->ocl),
                                                ocl_get_devices (data->ocl)[0],
                                                0, &errcode);
    OCL_CHECK_ERROR (errcode);

    data->write_queue = clCreateCommandQueue (ocl_get_context (data->ocl),
                                              ocl_get_devices (data->ocl)[0],
                                              0, &errcode);
    OCL_CHECK_ERROR (errcode);

    data->read_queue = data->write_queue;
}

static void
setup_three_queues (Data *data)
{
    cl_int errcode;

    data->compute_queue = clCreateCommandQueue (ocl_get_context (data->ocl),
                                                ocl_get_devices (data->ocl)[0],
                                                0, &errcode);
    OCL_CHECK_ERROR (errcode);

    data->write_queue = clCreateCommandQueue (ocl_get_context (data->ocl),
                                              ocl_get_devices (data->ocl)[0],
                                              0, &errcode);
    OCL_CHECK_ERROR (errcode);

    data->read_queue = clCreateCommandQueue (ocl_get_context (data->ocl),
                                             ocl_get_devices (data->ocl)[0],
                                             0, &errcode);
}

static void
run_benchmark (SetupQueueFunc setup,
               const char *fmt,
               Data *data)
{
    GTimer *timer;

    setup (data);
    timer = g_timer_new ();
    execute_kernel (data, N_ITERATIONS);
    g_timer_stop (timer);
    g_print (fmt, g_timer_elapsed (timer, NULL));
    g_timer_destroy (timer);
    teardown_queues (data);
}

int
main (int argc, char const* argv[])
{
    OclPlatform *ocl;
    Data *data;
    cl_int errcode;
    cl_event event;

    ocl = ocl_new (CL_DEVICE_TYPE_ALL, 0);

    if (ocl == NULL)
        return 1;

    data = setup_data (ocl, 4096 * 2048);

    run_benchmark (setup_single_blocking_queue, "Single blocking queue: %fs\n", data);
    run_benchmark (setup_ooo_queue, "Single out-of-order queue: %fs\n", data);
    run_benchmark (setup_two_queues, "Two queues: %fs\n", data);
    run_benchmark (setup_three_queues, "Three queues: %fs\n", data);

    free_data (data);
    ocl_free (ocl);

    return 0;
}
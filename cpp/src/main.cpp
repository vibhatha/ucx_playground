#include <iostream>
#include <ucs/type/status.h>
#include <ucp/api/ucp.h>

int hello_ucx() {
    // Initialize the UCP context
    ucp_config_t *config;
    ucp_params_t ucp_params;
    ucp_context_h ucp_context;

    // Set parameters
    ucp_params.field_mask = UCP_PARAM_FIELD_FEATURES;
    ucp_params.features = UCP_FEATURE_TAG;

    // Read UCP configuration
    ucs_status_t status = ucp_config_read(NULL, NULL, &config);
    if (status != UCS_OK) {
        std::cerr << "Failed to read UCP config" << std::endl;
        return EXIT_FAILURE;
    }

    // Initialize UCP context
    status = ucp_init(&ucp_params, config, &ucp_context);
    ucp_config_release(config);
    if (status != UCS_OK) {
        std::cerr << "Failed to initialize UCP context" << std::endl;
        return EXIT_FAILURE;
    }

    // Print Hello World
    std::cout << "Hello World from UCX" << std::endl;

    // Clean up UCP context
    ucp_cleanup(ucp_context);
    return EXIT_SUCCESS;
}

int main() {
    std::cout << "Hello World" << std::endl;
    int status = hello_ucx();
    if(status != EXIT_SUCCESS) {
        std::cerr << "Failed to run hello_ucx" << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
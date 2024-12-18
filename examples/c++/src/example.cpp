//
// Copyright (c) 2024, Daily
//

#include "daily_rtvi.h"

#include <signal.h>

#include <fstream>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#define SLEEP_MS(ms) Sleep(ms)
#else
#include <unistd.h>
#define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

class App : public rtvi::RTVIEventCallbacks {
   public:
    App(const std::string& url, const nlohmann::json& config) : _running(true) {
        std::vector<std::string> headers;

        // SECURITY: Here we are using the Daily Bots API key directly to simplify
        // testing. However, we should be using an API token instead (e.g. JWT) to
        // connect to a custom web server. The custom server would hold the Daily
        // Bots API key and other service keys if needed (e.g. OpenAI, Cartesia,
        // etc.).
        const char* api_key = std::getenv("DAILY_BOTS_API_KEY");
        if (api_key) {
            std::stringstream auth;
            auth << "Authorization: Bearer " << api_key;
            headers.push_back(auth.str());
        }

        auto endpoints = rtvi::RTVIClientEndpoints {.connect = url};

        auto params = rtvi::RTVIClientParams {
                .endpoints = endpoints, .request = config, .headers = headers
        };

        auto options =
                rtvi::RTVIClientOptions {.params = params, .callbacks = this};

        _client = std::make_unique<rtvi::DailyVoiceClient>(options);
    }

    virtual ~App() {}

    void run() {
        _client->initialize();
        _client->connect();

        while (_running) {
            SLEEP_MS(1000);
        }

        _client->disconnect();
    }

    void quit() { _running = false; }

    // RTVIEventCallbacks

    void on_connected() override {
        std::cout << std::endl << ">>> Client connected" << std::endl;
    }

    void on_disconnected() override {
        std::cout << std::endl << ">>> Client disconnected" << std::endl;
    }

    void on_bot_connected(const nlohmann::json& bot) override {
        std::cout << std::endl << ">>> Bot connected" << std::endl;
    }

    void on_bot_disconnected(
            const nlohmann::json& bot,
            const std::string& reason
    ) override {
        std::cout << std::endl
                  << ">>> Bot disconnected: " << reason << std::endl;
    }

    void on_bot_ready() override {
        std::cout << std::endl << ">>> Bot ready" << std::endl;

        // Send an initial action as an example.
        nlohmann::json action = rtvi::RTVIMessage::action(
                "llm", "append_to_messages", nlohmann::json::parse(R"([
                  {
                    "name": "messages",
                    "value": [{
                      "role": "system",
                      "content": "Tell me a story about cats and dragons."
                    }]
                  },
                  { "name": "run_immediately", "value": true }
                ])")
        );

        _client->send_action(action);
    }

    void on_bot_started_speaking() override {
        std::cout << std::endl << ">>> Bot started speaking" << std::endl;
    }

    void on_bot_stopped_speaking() override {
        std::cout << std::endl << ">>> Bot stopped speaking" << std::endl;
    }

    void on_bot_transcript(const rtvi::BotTranscriptData& data) override {
        std::cout << std::endl
                  << ">>> Bot transcript: " << data.text << std::endl;
    }

    void on_bot_tts_text(const rtvi::BotTTSTextData& data) override {
        // Words as they are being spoken by the bot.
    }

    void on_bot_llm_text(const rtvi::BotLLMTextData& data) override {
        // LLM tokens.
    }

   private:
    std::atomic<bool> _running;
    std::unique_ptr<rtvi::RTVIClient> _client;
};

static std::unique_ptr<App> app;

static void signal_handler(int signum) {
    std::cout << std::endl;
    std::cout << "Interrupted!" << std::endl;
    std::cout << std::endl;
    if (app) {
        app->quit();
    }
}

static void usage() {
    std::cout << "Usage: example -b URL -c CONFIG_FILE" << std::endl;
    std::cout << "  -b    Daily Bots URL" << std::endl;
    std::cout << "  -c    Configuration file" << std::endl;
}

int main(int argc, char* argv[]) {
    char* url = nullptr;
    char* config_file = nullptr;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) {
            url = argv[++i];
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            config_file = argv[++i];
        } else {
            usage();
            return EXIT_FAILURE;
        }
    }

    if (url == nullptr || config_file == nullptr) {
        usage();
        return EXIT_SUCCESS;
    }

    std::ifstream input_file(config_file);
    if (!input_file.is_open()) {
        std::cerr << std::endl
                  << "ERROR: unable to open config file: " << config_file
                  << std::endl;
        return EXIT_FAILURE;
    }

    nlohmann::json config_json;

    try {
        input_file >> config_json;
        input_file.close();
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << std::endl
                  << "ERROR: invalid config file: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    try {
        app = std::make_unique<App>(url, config_json);
        app->run();
    } catch (const std::exception& ex) {
        std::cerr << std::endl
                  << "ERROR: error running the app: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

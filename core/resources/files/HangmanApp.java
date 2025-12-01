package academy.app;

import academy.core.GameDifficulty;
import academy.core.GameEngine;
import academy.dictionary.DictionaryLoader;
import academy.dictionary.GameDictionary;
import academy.dictionary.Word;
import academy.view.ConsoleUI;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.ObjectReader;
import com.fasterxml.jackson.dataformat.yaml.YAMLFactory;
import java.io.File;
import java.io.IOException;
import java.io.UncheckedIOException;
import java.util.function.Predicate;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import picocli.CommandLine;
import picocli.CommandLine.Command;
import picocli.CommandLine.Option;
import picocli.CommandLine.Parameters;
import static java.util.Objects.nonNull;

@Command(name = "Application Example", version = "Example 1.0", mixinStandardHelpOptions = true)
public class HangmanApp implements Runnable {

    private static final Logger log = LoggerFactory.getLogger(HangmanApp.class);
    private static final ObjectReader YAML_READER =
        new ObjectMapper(new YAMLFactory()).findAndRegisterModules().reader();
    private static final Predicate<String[]> IS_TESTING_MODE = words -> nonNull(words) && words.length == 2;
    private static final int RETRY_MAX_ATTEMPTS = 3;
    private static final long RETRY_BACKOFF_MS = 200L;

    @Option(
        names = {"-s", "--font-size"},
        description = "Font size")
    int fontSize;

    @Parameters(
        paramLabel = "<word>",
        description = "Words pair for testing mode")
    private String[] words;

    @Option(
        names = {"-c", "--config"},
        description = "Path to YAML config file")
    private File configPath;

    public static void main(String[] args) {
        int exitCode = new CommandLine(new HangmanApp()).execute(args);
        System.exit(exitCode);
    }

    @Override
    public void run() {
        log.info("The application is running");

        AppConfig config = loadConfig();
        log.atInfo().addKeyValue("config", config).log("Config content");

        if (IS_TESTING_MODE.test(config.words())) {
            log.info("Non-interactive testing mode enabled");

            String targetWord = config.words()[0];
            String guessWord = config.words()[1];
            GameEngine gameEngine = new GameEngine(new Word(targetWord), GameDifficulty.NOOB);
            GameSimulator gameSimulator = new GameSimulator(gameEngine, guessWord);
            log.info("A simulation of the game has been launched, targetWord={}, guessWord={}",
                targetWord, guessWord);
            String result = gameSimulator.gameSimulation();

            System.out.println(result);
        } else {
            log.info("Interactive mode enabled");

            ConsoleUI ui = new ConsoleUI();
            ui.welcomeMessage();
            GameDictionary gameDictionary;

            if (config.words() == null) {
                gameDictionary = DictionaryLoader.loadDictionaryWithRetry(RETRY_MAX_ATTEMPTS, RETRY_BACKOFF_MS);
            } else {
                gameDictionary = GameDictionary.createFromConfig(config);
            }

            while (true) {
                System.out.println("=== A NEW GAME SESSION HAS BEEN LAUNCHED ===");
                GameDifficulty gameDifficulty = ui.chooseDifficulty();
                String wordsCategory = ui.chooseCategory(gameDictionary);
                GameEngine gameEngine = new GameEngine(
                    gameDictionary.getRandomWordFromCategory(wordsCategory), gameDifficulty
                );
                log.info("The game session has started");
                ui.startGameLoop(gameEngine);

                if (!ui.continueGame()) {
                    log.info("The app has finished working");
                    return;
                }

            }
        }
    }

    private AppConfig loadConfig() {

        if (configPath == null) return new AppConfig(fontSize, words);

        try {
            return YAML_READER.readValue(configPath, AppConfig.class);
        } catch (IOException e) {
            throw new UncheckedIOException(e);
        }
    }


}

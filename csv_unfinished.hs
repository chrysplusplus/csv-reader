import Control.Exception
import Data.Foldable
import System.Console.GetOpt
import System.Environment
import System.Exit
import System.IO

-- ===================
-- Commandline Parsing
-- ===================

usage_header = "Usage csv [options...] file"

data Filters = NoFilters | Filters [String]
  deriving Show

data InputFile = UsePath String | UseHandle Handle
  deriving Show

data Options = Options
  {
    getFilters :: Filters,
    getInputFile :: InputFile
  } deriving Show

defaultOptions = Options
  {
    getFilters = NoFilters,
    getInputFile = UseHandle stdin
  }

splitByP :: (a -> Bool) -> [a] -> [[a]]
splitByP p xs = case dropWhile p xs of
  []   -> []
  xs' -> pre : splitByP p xs''
    where (pre, xs'') = break p xs'

appendFilter :: String -> Options -> IO Options
appendFilter filter opts = return $ opts { getFilters = Filters new_filters }
  where
    new_filters = case getFilters opts of
      NoFilters           -> parsed_filters
      Filters old_filters -> old_filters ++ parsed_filters
    parsed_filters = splitByP (== ',') filter

appendNonOpts :: [String] -> Options -> IO Options
appendNonOpts xs opts
  | len == 0  = return opts
  | len == 1  = return $ opts { getInputFile = UsePath $ head xs }
  | otherwise = ioError . userError $ concat ["Too many files\n"] ++ usageInfo usage_header prog_opts
  where len = length xs

printHelp :: Options -> IO Options
printHelp _ = do
  putStr $ usageInfo usage_header prog_opts
  exitWith $ ExitFailure 1

prog_opts :: [OptDescr (Options -> IO Options)]
prog_opts = [
    Option ['f'] ["filter"] (ReqArg appendFilter "filter") "Filter by column",
    --Option ['l'] ["list"] (NoArg)
    Option ['h'] ["help"] (NoArg printHelp) "Print help documentation"
  ]

parseOpts :: [String] -> IO Options
parseOpts prog_args = do
  case getOpt Permute prog_opts prog_args of
    (opts, paths, []) -> foldrM (\o acc -> o acc) defaultOptions $ reverse (opts ++ [appendNonOpts paths])
    (_, _, errors)    -> ioError . userError $ concat errors ++ usageInfo usage_header prog_opts

-- =============
-- File Handling
-- =============

data FileHandle = FileHandle {
  getPath   :: Maybe String,
  getHandle :: Handle,
  getClose  :: FileHandle -> IO ()
}

instance Show FileHandle where
  show FileHandle { getPath = Nothing, getHandle = handle } = "FileHandle " ++ show handle
  show FileHandle { getPath = Just path } = "FileHandle " ++ show path

closeFileHandle :: FileHandle -> IO ()
closeFileHandle h = (getClose h) h

readFromFileHandle :: FileHandle -> IO String
readFromFileHandle = hGetContents . getHandle

openInputFile :: InputFile -> IO FileHandle
openInputFile (UsePath path) = do
  handle <- openFile path ReadMode
  return $ FileHandle { getPath = Just path, getHandle = handle, getClose = hClose . getHandle }

openInputFile (UseHandle handle) = do
  return $ FileHandle { getPath = Nothing, getHandle = handle, getClose = (\_ -> return ()) }

-- ===========
-- CSV Parsing
-- ===========

data CSV = CSV {
  getColumns :: [String],
  getRecords :: [[String]]
} deriving Show

data CSVParserState = CSVFieldUnquoted | CSVFieldQuoted

splitFields :: String -> [String]
splitFields = collect . foldl accFields ([], "", CSVFieldUnquoted)
  where
    accFields (acc, this, state) x = case (state, x) of
      (CSVFieldUnquoted, '\"') -> (acc, this, CSVFieldQuoted)
      (CSVFieldUnquoted, ',')  -> (append acc this, "", state)
      (CSVFieldQuoted, '\"')   -> (acc, this, CSVFieldUnquoted)
      _                        -> (acc, append this x, state)
    append acc x = acc ++ [x]
    collect (acc, this, _) = append acc this

fileToCSV :: String -> CSV
fileToCSV = splitHeadingRecords . lines
  where
    splitHeadingRecords []     = CSV { getColumns = [], getRecords = [] }
    splitHeadingRecords (x:xs) = CSV {
      getColumns = splitFieldsClean x,
      getRecords = map splitFieldsClean xs
    }
    splitFieldsClean = splitFields . takeWhile (/= '\r')

-- =============
-- CSV Filtering
-- =============

--columnIndex :: String -> CSV -> Maybe Int
columnIndex column_name = find (== column_name)

-- ============
-- Main Program
-- ============

withCmdlineFile :: Options -> (FileHandle -> IO c) -> IO c
withCmdlineFile opts = bracket (openInputFile $ getInputFile opts) closeFileHandle

do_prog :: [String] -> IO ()
do_prog prog_args = do
  opts <- parseOpts prog_args
  withCmdlineFile opts (\file -> do
    contents <- readFromFileHandle file
    let csv = fileToCSV contents
    print opts
    print csv)

main :: IO ()
main = do
  prog_args <- getArgs
  opts <- parseOpts prog_args
  withCmdlineFile opts (\file -> do
    contents <- readFromFileHandle file
    putStr contents)

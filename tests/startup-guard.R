# Checks on Build/startup/startup.R, the file loaded into every hosted R
# session. Run it against as many R versions as you can:
#
#   Rscript tests/startup-guard.R
#
# Exits non-zero on failure, so CI notices.

args <- commandArgs(trailingOnly = TRUE)
startup <- if (length(args)) args[1] else file.path("Build", "startup", "startup.R")

cat("R", as.character(getRversion()), "-", startup, "\n\n")

ok <- TRUE
check <- function(label, pass, detail = "") {
  cat(sprintf("  %-52s %s %s\n", label, if (pass) "ok  " else "FAIL", detail))
  if (!pass) ok <<- FALSE
}

# ---------------------------------------------------------------- it parses

parsed <- tryCatch(parse(startup), error = function(e) e)
check("startup.R parses", !inherits(parsed, "error"),
      if (inherits(parsed, "error")) conditionMessage(parsed) else "")
if (!ok) quit(status = 1)

# ------------------------------------------------- no vector in a condition
#
# R warns from 4.2 and refuses from 4.3 when `||` or `&&` is handed something
# longer than one element. This file drives R's completion machinery, where
# the values are vectors more often than not, so it is an easy mistake to
# make and it lands in the shell on every keystroke.

find_calls <- function(node, ops = c("||", "&&"), found = list()) {
  if (is.call(node)) {
    head <- node[[1]]
    if (is.name(head) && as.character(head) %in% ops) found[[length(found) + 1]] <- node
    for (i in seq_along(node)) {
      # an argument can be the empty symbol -- x[, 1] and friends -- which
      # errors as soon as it is looked at, so guard every touch of it
      part <- tryCatch(node[[i]], error = function(e) NULL)
      is_code <- tryCatch(!is.null(part) && (is.call(part) || is.pairlist(part)),
                          error = function(e) FALSE)
      if (isTRUE(is_code)) found <- find_calls(part, ops, found)
    }
  }
  found
}

conditions <- list()
for (e in parsed) conditions <- find_calls(e, found = conditions)

# an operand that is a bare comparison against a whole symbol is the shape
# that bit us: `n == ""` where n is a character vector
suspicious <- Filter(function(cl) {
  any(vapply(as.list(cl)[-1], function(operand) {
    if (!is.call(operand)) return(FALSE)
    op <- operand[[1]]
    if (!is.name(op) || !(as.character(op) %in% c("==", "!="))) return(FALSE)
    lhs <- tryCatch(operand[[2]], error = function(e) NULL)
    isTRUE(tryCatch(is.name(lhs), error = function(e) FALSE))  # a plain symbol, not x[1]
  }, logical(1)))
}, conditions)

check(sprintf("no whole-vector comparison in %d || and && conditions", length(conditions)),
      length(suspicious) == 0,
      if (length(suspicious)) paste(vapply(suspicious, function(x) paste(deparse(x), collapse = " "), ""), collapse = "; ") else "")

# ------------------------------------------- completion fails quietly, not loudly
#
# The completion code borrows R internals that carry no compatibility
# promise. If one of them changes, we want the argument hints to disappear,
# not errors on every keystroke.

find_def <- function(node, name) {
  if (is.call(node)) {
    if (length(node) >= 3 && identical(node[[1]], as.name("<-")) &&
        identical(node[[2]], as.name(name))) return(node[[3]])
    for (i in seq_along(node)) {
      part <- tryCatch(node[[i]], error = function(e) NULL)
      is_code <- tryCatch(!is.null(part) && (is.call(part) || is.pairlist(part)),
                          error = function(e) FALSE)
      if (isTRUE(is_code)) {
        found <- find_def(part, name)
        if (!is.null(found)) return(found)
      }
    }
  }
  NULL
}

get_def <- function(name) {
  for (e in parsed) {
    d <- find_def(e, name)
    if (!is.null(d)) return(d)
  }
  NULL
}

autocomplete <- get_def(".Autocomplete")
completer <- get_def(".CustomCompleter")

check("the console entry points are there", !is.null(autocomplete) && !is.null(completer))

if (!is.null(autocomplete)) {
  env <- new.env()
  env$.AutocompleteImpl <- function(...) stop("pretend a future R renamed an internal")
  env$.Autocomplete <- eval(autocomplete, env)
  result <- tryCatch(env$.Autocomplete("paste0(", 7L),
                     error = function(e) structure(conditionMessage(e), class = "threw"))
  check("a broken completer does not throw at the console", !inherits(result, "threw"),
        if (inherits(result, "threw")) as.character(result) else "")
  if (is.list(result)) {
    wanted <- c("comps", "function.signature", "token", "fguess", "start", "end", "in.quotes")
    check("it still answers in the shape the console reads", all(wanted %in% names(result)),
          paste(setdiff(wanted, names(result)), collapse = " "))
  }
  env$.AutocompleteImpl <- function(...) list(comps = "paste0", function.signature = "paste0(...)")
  check("a working completer is passed through untouched",
        identical(env$.Autocomplete("x", 1L)$comps, "paste0"))
}

if (!is.null(completer)) {
  env2 <- new.env()
  env2$.CustomCompleterImpl <- function(ce) stop("pretend .CompletionEnv changed shape")
  env2$.CustomCompleter <- eval(completer, env2)
  ce <- new.env()
  ce[["comps"]] <- "stale"
  ce[["function.signature"]] <- "stale"
  threw <- tryCatch({ env2$.CustomCompleter(ce); FALSE }, error = function(e) TRUE)
  check("a broken custom completer does not throw", !threw)
  check("and it clears what it could not work out",
        identical(ce[["comps"]], character()) && identical(ce[["function.signature"]], ""))
}

# ------------------------------------------------------- the completion cap

limit <- get_def(".completion.limit")
check("completions are capped", !is.null(limit) && is.numeric(eval(limit)) && eval(limit) > 0,
      if (is.null(limit)) "no .completion.limit" else paste("limit", eval(limit)))

cat("\n", if (ok) "all checks passed" else "FAILURES", "\n", sep = "")
quit(status = if (ok) 0 else 1)

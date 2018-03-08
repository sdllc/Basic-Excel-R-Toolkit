/**
 * Copyright (c) 2017-2018 Structured Data, LLC
 * 
 * This file is part of BERT.
 *
 * BERT is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * BERT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with BERT.  If not, see <http://www.gnu.org/licenses/>.
 */

/** escape sequence (escape-brace) */
export const VTESC = `\x1b[`;

/** utility function: split keywords, add or (|) and word-break */
const KeywordsToRegex = function(keywords:string) {
  return new RegExp( `\\b(${keywords.replace(/\s+/g, "|")})\\b`, "g" );
}

export interface Token {
  type:string;
  value:string;
}

export interface TokenizedString {
  text: string;
  tokens: Token[];
}

export interface TextFormatter {
  FormatString(string): string;  
}

export class RTextFormatter {

  /**
   * formalize:
   * 0
   * 0.0
   * 0xA
   * 0e+1 
   */
  private number_regex_ = /(\s|^|\W)(\d+\.\d+|0x[\dA-F]+|\d+e[\+-]\d+|\d+e[\+-]\d+\.\d+|\d+)\b/ig;

  /**
   * ordered by importance
   */
  private keyword_regex_ = [
    KeywordsToRegex("if else repeat while function for in next break TRUE FALSE NULL NA Inf NaN"),
    KeywordsToRegex("namespace bytecode abbreviate abline abs acf acos acosh addmargins aggregate agrep alarm alias alist all anova any aov aperm append apply approx approxfun apropos ar args arima array arrows asin asinh assign assocplot atan atanh attach attr attributes autoload autoloader ave axis backsolve barplot basename beta bindtextdomain binomial biplot bitmap bmp body box boxplot bquote break browser builtins bxp by bzfile c call cancor capabilities casefold cat category cbind ccf ceiling character charmatch chartr chol choose chull citation class close cm cmdscale codes coef coefficients col colnames colors colorspaces colours comment complex confint conflicts contour contrasts contributors convolve cophenetic coplot cor cos cosh cov covratio cpgram crossprod cummax cummin cumprod cumsum curve cut cutree cycle data dataentry date dbeta dbinom dcauchy dchisq de debug debugger decompose delay deltat demo dendrapply density deparse deriv det detach determinant deviance dexp df dfbeta dfbetas dffits dgamma dgeom dget dhyper diag diff diffinv difftime digamma dim dimnames dir dirname dist dlnorm dlogis dmultinom dnbinom dnorm dotchart double dpois dput drop dsignrank dt dump dunif duplicated dweibull dwilcox eapply ecdf edit effects eigen emacs embed end environment eval evalq example exists exp expression factanal factor factorial family fft fifo file filter find fitted fivenum fix floor flush for force formals format formula forwardsolve fourfoldplot frame frequency ftable function gamma gaussian gc gcinfo gctorture get getenv geterrmessage gettext gettextf getwd gl glm globalenv gray grep grey grid gsub gzcon gzfile hat hatvalues hcl hclust head heatmap help hist history hsv httpclient iconv iconvlist identical identify if ifelse image influence inherits integer integrate interaction interactive intersect invisible isoreg jitter jpeg julian kappa kernapply kernel kmeans knots kronecker ksmooth labels lag lapply layout lbeta lchoose lcm legend length letters levels lfactorial lgamma library licence license line lines list lm load loadhistory loadings local locator loess log logb logical loglin lowess ls lsfit machine mad mahalanobis makepredictcall manova mapply match matlines matplot matpoints matrix max mean median medpolish menu merge message methods mget min missing mode monthplot months mosaicplot mtext mvfft names napredict naprint naresid nargs nchar ncol next nextn ngettext nlevels nlm nls noquote nrow numeric objects offset open optim optimise optimize options order ordered outer pacf page pairlist pairs palette par parse paste pbeta pbinom pbirthday pcauchy pchisq pdf pentagamma person persp pexp pf pgamma pgeom phyper pi pico pictex pie piechart pipe plclust plnorm plogis plot pmatch pmax pmin pnbinom png pnorm points poisson poly polygon polym polyroot postscript power ppoints ppois ppr prcomp predict preplot pretty princomp print prmatrix prod profile profiler proj promax prompt provide psigamma psignrank pt ptukey punif pweibull pwilcox q qbeta qbinom qbirthday qcauchy qchisq qexp qf qgamma qgeom qhyper qlnorm qlogis qnbinom qnorm qpois qqline qqnorm qqplot qr qsignrank qt qtukey quantile quarters quasi quasibinomial quasipoisson quit qunif quote qweibull qwilcox rainbow range rank raw rbeta rbind rbinom rcauchy rchisq readline real recover rect reformulate regexpr relevel remove reorder rep repeat replace replicate replications require reshape resid residuals restart return rev rexp rf rgamma rgb rgeom rhyper rle rlnorm rlogis rm rmultinom rnbinom rnorm round row rownames rowsum rpois rsignrank rstandard rstudent rt rug runif runmed rweibull rwilcox sample sapply save savehistory scale scan screen screeplot sd search searchpaths seek segments seq sequence serialize setdiff setequal setwd shell sign signif sin single sinh sink smooth solve sort source spectrum spline splinefun split sprintf sqrt stack stars start stderr stdin stdout stem step stepfun stl stop stopifnot str strftime strheight stripchart strptime strsplit strtrim structure strwidth strwrap sub subset substitute substr substring sum summary sunflowerplot supsmu svd sweep switch symbols symnum system t table tabulate tail tan tanh tapply tempdir tempfile termplot terms tetragamma text time title toeplitz tolower topenv toupper trace traceback transform trigamma trunc truncate try ts tsdiag tsp typeof unclass undebug union unique uniroot unix unlink unlist unname unserialize unsplit unstack untrace unz update upgrade url var varimax vcov vector version vi vignette warning warnings weekdays weights which while window windows with write wsbrowser xedit xemacs xfig xinch xor xtabs xyinch yinch zapsmall"),
    KeywordsToRegex("acme aids aircondit amis aml banking barchart barley beaver bigcity boot brambles breslow bs bwplot calcium cane capability cav censboot channing city claridge cloth cloud coal condense contourplot control corr darwin densityplot dogs dotplot ducks empinf envelope environmental ethanol fir frets gpar grav gravity grob hirose histogram islay knn larrows levelplot llines logit lpoints lsegments lset ltext lvqinit lvqtest manaus melanoma melanoma motor multiedit neuro nitrofen nodal ns nuclear oneway parallel paulsen poisons polar qq qqmath remission rfs saddle salinity shingle simplex singer somgrid splom stripplot survival tau tmd tsboot tuna unit urine viewport wireframe wool xyplot")
  ];
  
  /**
   * the strategy here is to first remove strings and 
   * comments, then format keywords and numbers, then
   * put the strings and comments back. 
   */
  FormatString(line: string): string {

    let {text, tokens} = this.TokenizeStrings(line);

    text = text.replace(this.number_regex_, `$1${VTESC}33m$2${VTESC}0m`)
    text = text.replace(this.keyword_regex_[0], `${VTESC}34m$1${VTESC}0m`);
    text = text.replace(this.keyword_regex_[1], `${VTESC}35m$1${VTESC}0m`);

    tokens.forEach((token, index) => {
      let color = 30;
      if (token.type === 'string') color = 31;
      else if (token.type === 'comment') color = 32;
      text = text.replace(`___TOKEN___${index}___`, `${VTESC}${color}m${token.value}${VTESC}0m`)
    });

    return text;

  }

  /**
   * tokenize strings and R comments using a state machine.
   */
  TokenizeStrings(line: string): TokenizedString {

    let len = line.length;
    let copy = line;
    let s = "";
    let state: number | string = 0;
    let escaped = false;
    let tokens:Token[] = [];

    for (let i = 0; i < len; i++) {

      let ch = line[i];
      switch (state) {
        case 0:
          switch (ch) {
            case '#':
              s = state = ch;
              break;
            case '"':
            case "'":
              s = ch;
              state = ch;
              break;
          }
          break;

        case '#':
          s += ch;
          if (i === (len - 1)) {
            state = 0;
            copy = copy.replace(s, `___TOKEN___${tokens.length}___`)
            tokens.push({ type: "comment", value: s });
          }
          break;

        case "'":
        case '"':
          s += ch;
          if (ch === state && !escaped) {
            state = 0;
            copy = copy.replace(s, `___TOKEN___${tokens.length}___`)
            tokens.push({ type: "string", value: s });
          }
          else if (ch === '\\' && !escaped) {
            escaped = true;
          }
          else escaped = false;
          break;

      }
    }

    return { text:copy, tokens };
  }

}



/**
 * this is based on 
 * 
 * https://github.com/soulwire/FontMetrics
 * https://pomax.github.io/fontmetrics.js/fontmetrics.js
 * 
 * both of which seem reasonable but aren't exactly what we need
 */

/**
 * returned font metrics
 */
export interface Metrics {
  ascent:number;
  descent:number;
  width:number;
  text?:string;
}

/** 
 * this is not static, or global, because there's a possibility
 * we are (1) running async, and (2) running in different shells;
 * in that case we want to ensure separation. 
 */
export class FontMetrics {

  canvas_:HTMLCanvasElement;
  context_:CanvasRenderingContext2D;
  padding_ = 0;
  font_size_ = 0;
  cached_font_:string;

  public get context() { return this.context_ }

  constructor(){
    this.canvas_ = document.createElement("canvas");
    this.context_ = this.canvas_.getContext("2d");
  }

  SetFont(font_family, font_size:number|string, font_weight = 400) {

    let font = `${font_weight} ${font_size} ${font_family}`;
    if( this.cached_font_ === font ){
      // console.info("font match");
      return;
    }

    if( typeof font_size === "number" ){
      this.font_size_ = font_size;
    }
    else {
      this.font_size_ = Number(font_size.replace(/[^\d\.]/g, ""))
    }

    this.padding_ = this.font_size_ * 0.5;
    this.canvas_.width = this.font_size_ * 2;
    this.canvas_.height = this.font_size_ * 2 + this.padding_;
    this.context_.textBaseline = 'top'
    this.context_.textAlign = 'center'
    this.context_.font = font;

    this.cached_font_ = font;
    
  }

  UpdateText(char) {
    this.context_.clearRect(0, 0, this.canvas_.width, this.canvas_.height);
    this.context_.fillText(char, this.canvas_.width / 2, this.padding_, this.canvas_.width)
  }

  GetPixels(char) {
    this.UpdateText(char);
    return this.context_.getImageData(0, 0, this.canvas_.width, this.canvas_.height).data;
  }
  
  GetFirstIndex(pixels) {
    for (let i = 3, n = pixels.length; i < n; i += 4) {
      if (pixels[i] > 0) return (i - 3) / 4;
    } 
    return pixels.length;
  }
  
  GetLastIndex(pixels) {
    for (let i = pixels.length - 1; i >= 3; i -= 4) {
      if (pixels[i] > 0) return i / 4;
    } 
    return 0;
  }

  MeasureTop(char){
    return Math.round(this.GetFirstIndex(this.GetPixels(char)) / this.canvas_.width) - this.padding_;
  }
  
  MeasureBottom(char){
    return Math.round(this.GetLastIndex(this.GetPixels(char)) / this.canvas_.width) - this.padding_;
  }
 
  Measure(char) : Metrics {

    // use for width
    let text_metrics = this.context_.measureText(char);

    // use this one for baseline
    let baseline = this.MeasureBottom('n');

    // now measure the actual char
    let ascent = this.MeasureTop(char);
    let descent = this.MeasureBottom(char);
  
    //console.info("TM", char, baseline, baseline - ascent, descent - baseline, this.cached_font_)

    return {
      ascent: (baseline - ascent),
      descent: Math.max(0, descent - baseline),
      width: text_metrics.width,
      text: char
    };

  }
  
}


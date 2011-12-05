import graph;

locale("C");

real x[] = { 0.10, 0.15, 0.20, 0.25, 0.30, 0.35, 0.40, 0.45, 0.50, 0.55, 0.60, 0.65, 0.70};
include "./all24.data";

picture[] pic;

void plof(Label ll, real yb[], real yo[], real yc[], pen penna){
  picture tmppic;
  currentpicture=tmppic;
  size(x=8cm,y=6cm,IgnoreAspect);

 
  yb=1e6*yb; yo=1e6*yo; yc=1e6*yc;
  real xb[];
  for (int i=0; i<yb.length; ++i)
    xb[i]=x[i];
  real xo[];
  for (int i=0; i<yo.length; ++i)
    xo[i]=x[i];
  real xc[];
  for (int i=0; i<yc.length; ++i)
    xc[i]=x[i];
  draw(graph(xb,yb),penna);
  draw(graph(xo,yo),penna+dashed);
  draw(graph(xc,yc),penna+dotted);

  xlimits(min=0.1, max=0.7);
  ylimits(min=0, max=100, Crop);
  xaxis("Injection rate", BottomTop, LeftTicks);
  yaxis("Average latency ($\mu$s)", LeftRight, RightTicks);
  label(ll,1.1*point(N),penna);
  
  pic.push(currentpicture);
};

plof(Label("Uniform"), unyb, unyo, unyc, red);
plof(Label("Butterfly"), buyb, buyo, buyc, green);
plof(Label("BitComplement"), biyb, biyo, biyc, blue);
plof(Label("Transposition"), tryb, tryo, tryc, magenta);
plof(Label("Tornado"), toyb, toyo, toyc, cyan);


picture show;
currentpicture=show;

for(int i=0; i<pic.length; ++i){
  add(pic[i].fit(),(0,0),E);
  newpage();
}


// add(scale(.45)*legend(),.67*point((1,-0.1)),UnFill);


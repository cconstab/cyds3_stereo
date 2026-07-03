# Self-contained firmware installer page (ESP Web Tools + baked-in firmware).
# Build via ./build.sh, which stages the current firmware-merged.bin and manifest.

# Stage 1: fetch esp-web-tools from npm
FROM node:22-alpine AS tools
WORKDIR /tools
RUN npm install --no-audit --no-fund esp-web-tools@10

# Stage 2: static site on nginx
FROM nginx:alpine
COPY site/ /usr/share/nginx/html/
COPY --from=tools /tools/node_modules/esp-web-tools/dist/ /usr/share/nginx/html/vendor/esp-web-tools/
# staged by build.sh:
COPY stage/manifest.json /usr/share/nginx/html/manifest.json
COPY stage/firmware-merged.bin /usr/share/nginx/html/firmware/firmware-merged.bin
EXPOSE 80

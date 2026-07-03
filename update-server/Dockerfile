FROM node:22-alpine
WORKDIR /app
ENV NODE_ENV=production
COPY package.json package-lock.json* ./
RUN npm install --omit=dev
COPY server.js ./
COPY public ./public
RUN mkdir -p /data && chown node:node /data
VOLUME /data
ENV DATA_DIR=/data PORT=8080
EXPOSE 8080
USER node
CMD ["node", "server.js"]
